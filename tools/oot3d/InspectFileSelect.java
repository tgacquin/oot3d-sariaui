// @category OoT3D
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.program.model.address.Address;
import ghidra.program.model.pcode.JumpTable;
import java.util.ArrayList;
import java.nio.file.Files;
import java.nio.file.Path;

public class InspectFileSelect extends GhidraScript {
    public void run() throws Exception {
        var function = getFunctionAt(toAddr(0x42f984));
        var destinations = new ArrayList<Address>();
        for (int i = 0; i < 48; ++i)
            destinations.add(toAddr(Integer.toUnsignedLong(getInt(toAddr(0x42f9e0 + i * 4)))));
        new JumpTable(toAddr(0x42f9d8), destinations, true, 0).writeOverride(function);
        var decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try {
            var result = decompiler.decompileFunction(function, 90, monitor);
            Files.writeString(Path.of(getScriptArgs()[0]), result.decompileCompleted()
                ? result.getDecompiledFunction().getC() : result.getErrorMessage());
        } finally { decompiler.dispose(); }
    }
}
