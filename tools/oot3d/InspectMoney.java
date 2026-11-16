// @category OoT3D
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class InspectMoney extends GhidraScript {
 public void run() throws Exception {
  try(var out=Files.newBufferedWriter(Path.of(getScriptArgs()[0]))) {
   for(long pc=0x100000;pc<0x4c0000;pc+=4) {
    int w=getInt(toAddr(pc));long rel=(w&0xffffff)*4; if((rel&0x2000000)!=0)rel-=0x4000000;
    boolean money=(w&0x0e000000)==0x0a000000 && pc+8+rel==0x376a60;
    boolean funds=(w&0x0ff00fff)==0x01d004f8 && (pc<0x1a0000 || pc>=0x3c0000 && pc<0x3d2000);
    if(!money && !funds)continue;
    out.write("\nSITE "+Long.toHexString(pc)+"\n");
    for(long at=pc-40;at<=pc+16;at+=4) {
     var addr=toAddr(at);var ins=getInstructionAt(addr);if(ins==null){disassemble(addr);ins=getInstructionAt(addr);}
     out.write(addr+" "+ins+"\n");
    }
   }
  }
 }
}
