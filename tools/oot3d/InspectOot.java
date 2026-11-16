// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import java.nio.file.Files;
import java.nio.file.Path;

public class InspectOot extends GhidraScript {
    public void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (var out = Files.newBufferedWriter(Path.of(args[0]))) {
            for (int i = 1; i < args.length; i++) {
                var address = toAddr(args[i]);
                out.write("\nADDRESS " + address + "\n");
                for (var ref : getReferencesTo(address)) {
                    var caller = getFunctionContaining(ref.getFromAddress());
                    out.write("REF " + ref.getFromAddress() + " " + ref.getReferenceType() + " " +
                              (caller == null ? "data" : caller.getEntryPoint()) + "\n");
                }
                var block = currentProgram.getMemory().getBlock(address);
                if (block == null || !block.isExecute()) continue;
                var function = getFunctionContaining(address);
                if (function == null) {
                    disassemble(address);
                    function = createFunction(address, null);
                }
                if (function == null) continue;
                var result = decompiler.decompileFunction(function, 30, monitor);
                if (result.decompileCompleted()) out.write(result.getDecompiledFunction().getC());
                for (var unit : currentProgram.getListing().getCodeUnits(function.getBody(), true)) {
                    out.write(unit.getAddress() + " " + unit + "\n");
                    for (var ref : unit.getReferencesFrom()) {
                        var data = getDataAt(ref.getToAddress());
                        if (data != null) out.write("  -> " + ref.getToAddress() + " " + data + "\n");
                    }
                }
            }
        } finally { decompiler.dispose(); }
    }
}
