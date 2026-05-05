import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.util.task.TaskMonitor;
import ghidra.program.model.data.*;

public class CallFunctionDecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        AddressFactory af = currentProgram.getAddressFactory();
        
        // Focus on 4 key functions
        String[] addrs = {"F814439C", "F814447C"};
        
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        
        for (String a : addrs) {
            Address addr = af.getAddress(a);
            Function f = currentProgram.getFunctionManager().getFunctionAt(addr);
            if (f == null) continue;
            
            println("=== " + f.getName() + " @ " + addr);
            DecompileResults res = decomp.decompileFunction(f, 120, TaskMonitor.DUMMY);
            if (res != null && res.decompileCompleted()) {
                String code = res.getDecompiledFunction().getC();
                println(code);
            } else if (res != null) {
                println("Decompile failed: " + res.getErrorMessage());
            }
        }
        
        decomp.dispose();
    }
}
