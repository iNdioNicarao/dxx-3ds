
cp ../d1x-3ds.elf .
bannertool makebanner -i banner.png -a jingle.wav -o banner.bnr
bannertool makesmdh -s "D1X 3DS (Stereo)" -l "Port of DXX-Rebirth to 3DS (stereo)" -p "El iNdioNicarao" -i icon.png -o icon.icn
makerom -f cia -o d1x-3ds.cia -DAPP_ENCRYPTED=false -rsf d1x-3ds.rsf -target t -exefslogo -elf d1x-3ds.elf -icon icon.icn -banner banner.bnr
