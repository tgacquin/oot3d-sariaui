// Export analysis indexes and a small set of title-screen candidate decompilations.
// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;

public class ExportOotUi extends GhidraScript {
    @Override
    public void run() throws Exception {
        Path root = Path.of(getScriptArgs()[0]);
        var functions = currentProgram.getFunctionManager();
        try (var out = Files.newBufferedWriter(root.resolve("functions.tsv"))) {
            out.write("address\tname\tbytes\n");
            for (Function function : functions.getFunctions(true)) {
                monitor.checkCancelled();
                out.write(function.getEntryPoint() + "\t" + function.getName() + "\t" +
                          function.getBody().getNumAddresses() + "\n");
            }
        }
        var titleFunctions = new LinkedHashMap<Address, Function>();
        try (var out = Files.newBufferedWriter(root.resolve("ui-references.tsv"))) {
            out.write("string_address\treference_address\tfunction\ttext\n");
            for (String line : Files.readAllLines(root.resolve("ui-strings.tsv"))) {
                if (line.startsWith("address\t")) continue;
                String[] fields = line.split("\t", 3);
                Address address = toAddr(fields[0]);
                var pending = new LinkedHashSet<Address>();
                var seen = new LinkedHashSet<Address>();
                pending.add(address);
                // Follow direct references and one level of pointer tables.
                for (int level = 0; level < 2; ++level) {
                    var next = new LinkedHashSet<Address>();
                    for (Address target : pending) {
                        if (!seen.add(target)) continue;
                        for (Reference reference : getReferencesTo(target)) {
                            Function function = functions.getFunctionContaining(reference.getFromAddress());
                            out.write(address + "\t" + reference.getFromAddress() + "\t" +
                                      (function == null ? "" : function.getEntryPoint()) + "\t" + fields[2] + "\n");
                            if (function == null) next.add(reference.getFromAddress());
                            else if (fields[2].toLowerCase().contains("title"))
                                titleFunctions.put(function.getEntryPoint(), function);
                        }
                    }
                    pending = next;
                }
            }
        }
        DecompInterface decompiler = new DecompInterface();
        try (var out = Files.newBufferedWriter(root.resolve("title-candidates.c"))) {
            out.write("/* Ghidra-generated pseudocode. Candidates based on title string references;\n" +
                      "   identities and UI roles are not yet verified. Not original source. */\n");
            decompiler.openProgram(currentProgram);
            int count = 0;
            for (Function function : titleFunctions.values()) {
                if (count++ >= 20) break;
                monitor.checkCancelled();
                out.write("\n/* " + function.getEntryPoint() + " " + function.getName() + " */\n");
                var result = decompiler.decompileFunction(function, 15, monitor);
                if (result.decompileCompleted()) out.write(result.getDecompiledFunction().getC());
                else out.write("/* Decompilation unavailable: " + result.getErrorMessage().replace("*/", "") + " */\n");
            }
        } finally {
            decompiler.dispose();
        }
        println("Exported " + functions.getFunctionCount() + " functions; " +
                titleFunctions.size() + " title-reference candidate functions.");
    }
}
