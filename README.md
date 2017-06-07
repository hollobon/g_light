Arduino source for controlling a small light used for putting a child to sleep and 
indicating when it's a suitable time to get up.

There are two buttons. One triggers "sleep mode", which lights all six lights red
and gradually fades them over a short period.

The circuit has a DS3231 real-time clock, which is used for "waking up" mode. This
lights up LEDs yellow sequentially, then performs a "rainbow" for a few seconds, and
finally lights up all six LEDs bright green, indicating that it's OK to get up now.
