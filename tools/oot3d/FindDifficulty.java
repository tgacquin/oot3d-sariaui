// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import java.nio.file.*;
import java.util.*;
public class FindDifficulty extends GhidraScript {
 public void run() throws Exception {
  var funcs=new TreeSet<ghidra.program.model.address.Address>();
  var instructions=currentProgram.getListing().getInstructions(true);
  while(instructions.hasNext()) {
   var ins=instructions.next();var str=ins.toString();boolean include=false;
   if((str.startsWith("ldrsh ")||str.startsWith("ldrh "))&&str.contains("#0x48]"))include=true;
   for(var ref:ins.getReferencesFrom()) {
    var data=getDataAt(ref.getToAddress());
    if(data!=null&&data.toString().contains("00588E58"))include=true;
   }
   if(include){var f=getFunctionContaining(ins.getAddress());if(f!=null)funcs.add(f.getEntryPoint());}
  }
  var dec=new DecompInterface();dec.openProgram(currentProgram);
  try(var out=Files.newBufferedWriter(Path.of(getScriptArgs()[0]))) {
   for(var addr:funcs) {var f=getFunctionAt(addr);out.write("\nADDRESS "+addr+"\n");
    var r=dec.decompileFunction(f,30,monitor);if(r.decompileCompleted())out.write(r.getDecompiledFunction().getC());}
  }finally{dec.dispose();}
 }
}
