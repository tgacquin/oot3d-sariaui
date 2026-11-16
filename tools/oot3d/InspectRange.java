// @category OoT3D
import ghidra.app.script.GhidraScript;
import java.nio.file.*;
public class InspectRange extends GhidraScript {
 public void run() throws Exception {
  var args=getScriptArgs();
  try(var out=Files.newBufferedWriter(Path.of(args[0]))) {
   for(int i=1;i<args.length;i+=2) {
    var at=toAddr(args[i]);var end=toAddr(args[i+1]);
    while(at.compareTo(end)<0) {
     var ins=getInstructionAt(at);if(ins==null){disassemble(at);ins=getInstructionAt(at);}
     out.write(at+" "+ins+"\n");at=at.add(4);
    }
   }
  }
 }
}
