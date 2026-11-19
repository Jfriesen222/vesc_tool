;; Start the CANopen node on vcan0 and keep the REPL alive.
;; run_can_tests.sh kills the REPL when the master is done.
(co-start "vcan0")
(sleep 3600)
