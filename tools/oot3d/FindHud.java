// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import java.nio.file.*;
import java.util.*;
public class FindHud extends GhidraScript {
 public void run() throws Exception {
  var funcs = new TreeSet<String>();
  var listing=currentProgram.getListing();
  var it=listing.getInstructions(true);
  while(it.hasNext()) {
   var ins=it.next();var str=ins.toString();
   if(ins.getAddress().getOffset()>0x2c0000 && (str.startsWith("ldrsh ")||str.startsWith("ldrh ")) && str.contains("#0x44]")) {
    var f=getFunctionContaining(ins.getAddress());if(f!=null) funcs.add(f.getEntryPoint().toString());
   }
  }
  var dec=new DecompInterface();dec.openProgram(currentProgram);
  try(var out=Files.newBufferedWriter(Path.of(getScriptArgs()[0]))) {
   for(var addr:funcs) {var f=getFunctionAt(toAddr(addr));out.write("\nADDRESS "+addr+"\n");var r=dec.decompileFunction(f,30,monitor);if(r.decompileCompleted())out.write(r.getDecompiledFunction().getC());}
  }finally{dec.dispose();}
 }
}
