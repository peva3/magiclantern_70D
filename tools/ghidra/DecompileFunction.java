/*Ghidra Script: Decompile a function at a given address and print its C code
 * Usage: analyzeHeadless <proj_dir> <proj_name> -process ROM1.BIN -postScript DecompileFunction.java <hex_addr> [label]
 *@category Canon70D
 */

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.util.task.TaskMonitor;

public class DecompileFunction extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("ERROR: Usage: DecompileFunction.java <hex_addr> [label]");
            return;
        }

        String addrStr = args[0];
        String label = (args.length > 1) ? args[1] : ("func_" + addrStr);

        if (addrStr.startsWith("0x") || addrStr.startsWith("0X")) {
            addrStr = addrStr.substring(2);
        }

        AddressFactory af = currentProgram.getAddressFactory();
        Address targetAddr;

        long val = Long.parseLong(addrStr, 16);
        if (val >= 0xFF000000L && val <= 0xFFFFFFFFL) {
            long romAddr = val - 0xFF000000L + 0xF8000000L;
            targetAddr = af.getAddress(Long.toHexString(romAddr).toUpperCase());
        } else {
            targetAddr = af.getAddress(addrStr.toUpperCase());
        }

        if (targetAddr == null) {
            println("ERROR: Invalid address: " + args[0]);
            return;
        }

        FunctionManager fm = currentProgram.getFunctionManager();
        Function f = fm.getFunctionAt(targetAddr);
        if (f == null) {
            f = fm.getFunctionContaining(targetAddr);
        }

        if (f == null) {
            println("ERROR: No function found at " + targetAddr);
            return;
        }

        println("=== " + label + " ===");
        println("Name: " + f.getName());
        println("Address: 0x" + targetAddr.toString(false));
        println("Body: " + f.getBody());

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);

        DecompileResults res = decomp.decompileFunction(f, 300, TaskMonitor.DUMMY);
        if (res != null && res.decompileCompleted()) {
            String code = res.getDecompiledFunction().getC();
            println(code);
        } else if (res != null) {
            println("Decompile FAILED: " + res.getErrorMessage());
            println("\n=== Instruction Listing ===");
            InstructionIterator iter = currentProgram.getListing().getInstructions(targetAddr, true);
            int count = 0;
            while (iter.hasNext() && count < 200) {
                Instruction instr = iter.next();
                if (!f.getBody().contains(instr.getAddress())) break;
                println(instr.toString());
                count++;
            }
        }

        decomp.dispose();
    }
}
