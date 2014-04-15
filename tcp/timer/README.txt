Run make to compile the delta timer as well as the driver.

Run 'dt'  to start the delta timer.  This will wait on an open socket for the driver to communicate.

Run 'td' to execute the timer creation and deletion tests.

After these tests are complete, the driver will remain open, and you may create more timers in the following manner.

Enter <mode> <port> <packet> <duration> on the command line.
<mode> = 1  for creating a new timer
<mode> = 2  for removing an old timer (used by tcpd when packet is ack'd)

<port> is the number of the port corresponding to the packet being timed
<packet> is the number of the packet being timed
<duration> is the duration of the timer in seconds
