// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.TreeSet;
public class InspectCallers extends GhidraScript {
 public void run() throws Exception {
  var args=getScriptArgs();var addresses=new TreeSet<ghidra.program.model.address.Address>();
  for(int i=1;i<args.length;i++)for(var ref:getReferencesTo(toAddr(args[i]))) {
   var f=getFunctionContaining(ref.getFromAddress());if(f!=null)addresses.add(f.getEntryPoint());
  }
  var decompiler=new DecompInterface();decompiler.openProgram(currentProgram);
  try(var out=Files.newBufferedWriter(Path.of(args[0]))) {
   for(var a:addresses) {var f=getFunctionAt(a);out.write("\nADDRESS "+a+"\n");
    var r=decompiler.decompileFunction(f,15,monitor);
    if(r.decompileCompleted())out.write(r.getDecompiledFunction().getC());
   }
  }finally{decompiler.dispose();}
 }
}
