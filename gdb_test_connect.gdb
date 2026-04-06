target remote localhost:61234
monitor halt
info reg pc sp lr
x/4xw 0x34180400
list *$pc
quit
