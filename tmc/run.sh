arm-none-eabi-size -A tmc.elf
arm-none-eabi-nm -S tmc.elf | awk '{printf("%s %s %s\n",$1,$2,$3)}' | sort -k2 -n -r | head -n 50
arm-none-eabi-objdump -h build/USA/src/maze_gen.o
arm-none-eabi-nm -S build/USA/src/maze_gen.o
grep -nE '\.data|\.bss' build/USA/tmc.map | sed -n '1,120p'
arm-none-eabi-objdump -t tmc.elf | egrep -i 'm4a|mplay|music|sound|snd|gMPlay|gSound' | sed -E 's/ +/ /g'
arm-none-eabi-nm -S tmc.elf | grep 020 | sort -k1
arm-none-eabi-objdump -h tmc.elf

strings tmc.gba | egrep -i 'GBAZELDA|M4A|m4a|SOUND|ZELDA|gbazelda' | sort -u
