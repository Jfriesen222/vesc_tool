;; Minimal CANopen slave: NMT, heartbeat, expedited SDO server.
;; Tested against python-canopen over vcan0, see run_can_tests.sh.

(define node-id 5)

;; NMT states as reported in heartbeat
(define nmt-bootup  0x00)
(define nmt-stopped 0x04)
(define nmt-oper    0x05)
(define nmt-preop   0x7F)
(define nmt-state nmt-preop)

;; Object dictionary: key = index*256 + subindex -> (type access value)
(defun odkey (idx sub) (+ (* idx 256) sub))

(define od (list
  (cons (odkey 0x1000 0) (list 'u32 'ro 0x00020192u32)) ; device type: CiA 402
  (cons (odkey 0x1001 0) (list 'u8  'ro 0))             ; error register
  (cons (odkey 0x1017 0) (list 'u16 'rw 500))           ; heartbeat ms
  (cons (odkey 0x1018 1) (list 'u32 'ro 0x0000ABCDu32)) ; vendor id
  (cons (odkey 0x2000 0) (list 'i32 'rw 0))))           ; user variable

(defun type-size (ty)
  (cond ((eq ty 'u8) 1)
        ((eq ty 'u16) 2)
        (t 4)))

(defun put-val (buf ty v)
  (cond ((eq ty 'u8)  (bufset-u8 buf 4 v))
        ((eq ty 'u16) (bufset-u16 buf 4 v 'little-endian))
        ((eq ty 'i32) (bufset-i32 buf 4 v 'little-endian))
        (t            (bufset-u32 buf 4 v 'little-endian))))

(defun get-val (buf ty)
  (cond ((eq ty 'u8)  (bufget-u8 buf 4))
        ((eq ty 'u16) (bufget-u16 buf 4 'little-endian))
        ((eq ty 'i32) (bufget-i32 buf 4 'little-endian))
        (t            (bufget-u32 buf 4 'little-endian))))

;; ---------------------------------------------------------------------------
;; SDO server (expedited only)

(defun sdo-frame (cmd idx sub)
  (let ((b (bufcreate 8)))
    (progn
      (bufset-u8 b 0 cmd)
      (bufset-u16 b 1 idx 'little-endian)
      (bufset-u8 b 3 sub)
      b)))

(defun sdo-send (b) (can-send-sid (+ 0x580 node-id) b))

(defun sdo-abort (idx sub code)
  (let ((b (sdo-frame 0x80 idx sub)))
    (progn
      (bufset-u32 b 4 code 'little-endian)
      (sdo-send b))))

(defun sdo-upload (idx sub entry)
  (let ((ty (ix entry 0))
        (n  (type-size ty))
        (b  (sdo-frame (bitwise-or 0x43 (shl (- 4 n) 2)) idx sub)))
    (progn
      (put-val b ty (ix entry 2))
      (sdo-send b))))

(defun sdo-download (cmd idx sub entry data)
  (let ((ty (ix entry 0))
        (size-given (= (bitwise-and cmd 0x01) 1))
        (n (- 4 (bitwise-and (shr cmd 2) 3))))
    (cond ((eq (ix entry 1) 'ro)
           (sdo-abort idx sub 0x06010002u32))   ; attempt to write read-only
          ((and size-given (not (= n (type-size ty))))
           (sdo-abort idx sub 0x06070010u32))   ; length mismatch
          (t (progn
               (setix entry 2 (get-val data ty))
               (sdo-send (sdo-frame 0x60 idx sub)))))))

(defun sdo-handle (data)
  (let ((cmd   (bufget-u8 data 0))
        (idx   (bufget-u16 data 1 'little-endian))
        (sub   (bufget-u8 data 3))
        (entry (assoc od (odkey idx sub))))
    (cond ((not entry)
           (sdo-abort idx sub 0x06020000u32))   ; object does not exist
          ((= cmd 0x40)
           (sdo-upload idx sub entry))
          ((= (bitwise-and cmd 0xE2) 0x22)      ; download, expedited
           (sdo-download cmd idx sub entry data))
          (t (sdo-abort idx sub 0x05040001u32))))) ; unsupported command

;; ---------------------------------------------------------------------------
;; NMT and heartbeat

(defun send-bootup () (can-send-sid (+ 0x700 node-id) (list nmt-bootup)))

(defun nmt-handle (data)
  (let ((cs     (bufget-u8 data 0))
        (target (bufget-u8 data 1)))
    (if (or (= target 0) (= target node-id))
        (cond ((= cs 0x01) (setq nmt-state nmt-oper))
              ((= cs 0x02) (setq nmt-state nmt-stopped))
              ((= cs 0x80) (setq nmt-state nmt-preop))
              ((or (= cs 0x81) (= cs 0x82))       ; reset node / communication
               (progn
                 (send-bootup)
                 (setq nmt-state nmt-preop)))
              (t nil))
        nil)))

(defun heartbeat ()
  (loopwhile t
    (progn
      (can-send-sid (+ 0x700 node-id) (list nmt-state))
      (sleep (/ (ix (assoc od (odkey 0x1017 0)) 2) 1000.0)))))

;; ---------------------------------------------------------------------------
;; Frame dispatch

(defun can-handler ()
  (loopwhile t
    (recv ((event-can-sid . ((? id) . (? data)))
           (cond ((= id 0) (nmt-handle data))
                 ((and (= id (+ 0x600 node-id))
                       (not (= nmt-state nmt-stopped))
                       (= (buflen data) 8))
                  (sdo-handle data))
                 (t nil)))
          (_ nil))))

(defun co-start (ifname)
  (progn
    (can-open ifname)
    (event-register-handler (spawn can-handler))
    (event-enable 'event-can-sid)
    (send-bootup)
    (spawn heartbeat)))
