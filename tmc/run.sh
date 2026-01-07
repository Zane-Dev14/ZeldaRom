# Deep-dive diagnostics (single bundle). Run from repo root.
# Outputs are truncated to keep total lines small; remove `| sed -n '1,XXXp'` if you want more.

echo "=== 1) Map lines placing .bss at 0x02037800 (brief) ==="
egrep -n "02037800" build/USA/tmc.map | sed -n '1,120p'
echo

echo "=== 2) Object files placed at 0x02037800 (unique list) ==="
egrep -n "02037800.+\\.o" build/USA/tmc.map \
  | sed -E 's/.* ([^ ]+\.o).*/\1/' \
  | sort -u \
  | sed -n '1,200p'
echo

echo "=== 4) Show any COMMON symbols in those object files (they become .bss at link-time) ==="
for o in $(egrep -n "02037800.+\\.o" build/USA/tmc.map | sed -E 's/.* ([^ ]+\.o).*/\1/' | sort -u); do
  echo "---- $o (COMMON search) ----"
  arm-none-eabi-nm -S "build/USA/$o" 2>/dev/null | egrep 'COMMON| B | b ' || true
done | sed -n '1,400p'
echo

echo "=== 5) Search source & asm for hard-coded EW-RAM addresses (0x0203xxxx / 0x02037xxx / 0x02036xxx) ==="
git grep -nE "0x0203[0-9a-fA-F]{3}|0x02037[0-9a-fA-F]{2}|0x02036[0-9a-fA-F]{2}" -- :/ 2>/dev/null | sed -n '1,200p'
echo

echo "=== 6) Search built object files for immediate constants referencing EW-RAM (movw/movt pairs / ldr literal pools) ==="
# Look for bytes/text containing '0203' or '02037' in immediate operands in disassembly
arm-none-eabi-objdump -d tmc.elf 2>/dev/null \
  | egrep -n "02036|02037|02038|0203[0-9a-fA-F]{3}" \
  | sed -n '1,400p'
echo

echo "=== 7) Check relocations that reference EW-RAM (relocs to 0x0203xxxx) ==="
readelf -r tmc.elf 2>/dev/null \
  | egrep -n "0x0203|0x036|02037" \
  | sed -n '1,400p' || true
echo

echo "=== 8) Show EW-RAM symbol addresses & sizes (focused) ==="
arm-none-eabi-nm -S tmc.elf \
  | egrep -i "gMPlayTracks|_mazeState_start|_mazeState_end|gEndOfEwram|gzHeap|gSoundPlayingInfo" \
  | sed -n '1,200p'
echo

echo "=== 9) Dump the .map region around 0x02036BC0..0x02038560 (context ±50 lines) ==="
ln=$(grep -n "02036bc0" build/USA/tmc.map | head -n1 | cut -d: -f1)
if [ -n "$ln" ]; then
  start=$(( ln > 50 ? ln-50 : 1 ))
  sed -n "${start},$((ln+120))p" build/USA/tmc.map | sed -n '1,400p'
else
  echo "map entry for 02036bc0 not found"
fi
echo

echo "=== 10) Check maze_gen.o for any unexpected sections (defensive double-check) ==="
arm-none-eabi-objdump -t build/USA/src/maze_gen.o | sed -n '1,200p'
arm-none-eabi-objdump -d build/USA/src/maze_gen.o | egrep -n "02036|02037|02038" | sed -n '1,200p' || true
echo

echo "=== 11) Quick check: are there any large .bss/.data consumers earlier than maze area? (top 30 .bss by size) ==="
arm-none-eabi-nm -S tmc.elf \
  | awk '/ [BDbd] / && $1 ~ /^02[0-9a-fA-F]+/ { printf "%s %s %s\n",$1,$2,$3 }' \
  | sort -k2 -nr \
  | sed -n '1,60p'
echo

echo "=== 12) If you suspect a runtime write to the maze block: show memory-mapped audio buffers & heap pointers (addresses) ==="
arm-none-eabi-nm -S tmc.elf | egrep "gMPlayTracks|gMPlayInfos|gMPlayMemAccArea|SoundMainRAM_Buffer|gzHeap" | sed -n '1,200p'
echo

# Guidance: paste the outputs (especially from steps 3,4,6,7) if you want me to interpret them.
# If the objdump relocation/disassembly (step 6/7) shows immediate 0x02037xxx usages, that means code/literals reference the reserved block directly.
# If many object files have COMMON/0-sized .bss in the map at 0x02037800, that's expected (they are linked after the reserved fill) —
# but disassembly/relocs referencing 0x02037000..0x02037800 would be the smoking gun (runtime writes / absolute refs).
