#!/bin/sh
#set a33i audio throughway begin
#dafult headphone out,default headphone volume is 30
echo "set a33i audio pass through"
amixer cset iface=MIXER,name='headphone volume' 30
amixer cset iface=MIXER,name='DACL Mixer AIF1DA0L Switch' 1
amixer cset iface=MIXER,name='DACR Mixer AIF1DA0R Switch' 1
amixer cset iface=MIXER,name='Headphone Switch' 1
#set r16 audio throughway finish
