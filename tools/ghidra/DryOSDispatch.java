// DryOSDispatch.java - Fast stub analysis only, no decompiler
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import ghidra.util.*;
import ghidra.program.model.mem.*;

public class DryOSDispatch extends GhidraScript {

    @Override
    public void run() throws Exception {
        println("=== DryOS Kernel Dispatch Analysis ===");

        long[][] stubs = {
            {0xFF0A577CL, 0x00000000L},  // CreateTask
            {0xFF0A583CL, 0x00000000L},  // CreateTask2
            {0xFF0A5A9CL, 0x00000000L},  // task_create
            {0xFF0A7F30L, 0x00000000L},  // msleep
            {0xFF0A83F0L, 0x00000000L},  // get_ms_clock
            {0xFF0A6E70L, 0x00000000L},  // get_current_task
            {0xFF0A6F20L, 0x00000000L},  // get_task_list
            {0xFF0A6680L, 0x00000000L},  // CreateStateObject
            {0xFF0A6AA0L, 0x00000000L},  // CreateMessageQueue
            {0xFF0A4360L, 0x00000000L},  // malloc
            {0xFF0A44F0L, 0x00000000L},  // free
            {0xFF0A89C0L, 0x00000000L},  // SetHPTimerAfterNow
            {0xFF0A8CF0L, 0x00000000L},  // SetTimerAfter
            {0xFF0E0B48L, 0x00000000L},  // FIO_OpenFile
            {0xFF0E10B8L, 0x00000000L},  // FIO_CloseFile
            {0xFF0E0F40L, 0x00000000L},  // FIO_ReadFile
            {0xFF0E1000L, 0x00000000L},  // FIO_WriteFile
            {0xFF0E0D28L, 0x00000000L},  // FIO_CreateFile
            {0xFF147F3CL, 0x00000000L},  // AllocateMemory
            {0xFF11AF60L, 0x00000000L},  // create_named_semaphore
            {0xFF0A7F24L, 0x00000000L},  // task_yield
            {0xFF0DD35CL, 0x00000000L},  // EngDrvOut
            {0xFF14439CL, 0x00000000L},  // call
        };

        for (long[] stub : stubs) {
            long addr = stub[0];
            Address a = toAddr(addr);

            if (!currentProgram.getMemory().contains(a)) {
                println(String.format("%-30s @ 0x%08X — NOT IN MEMORY", "", addr));
                continue;
            }

            Symbol[] syms = getSymbols(a);
            String name = (syms.length > 0) ? syms[0].getName() : "?";

            byte[] bytes = new byte[12];
            try { getBytes(a, bytes); } catch (Exception e) {
                println(String.format("%-30s @ 0x%08X — CANNOT READ", name, addr));
                continue;
            }

            int w0 = getInt(bytes, 0);
            int w1 = getInt(bytes, 4);
            int w2 = getInt(bytes, 8);

            // Check instruction(s) at this address
            Instruction instr = getInstructionAt(a);
            String instrStr;
            if (instr != null) {
                instrStr = instr.getMnemonicString();
                // Check for branches
                FlowReference[] flows = instr.getFlowsFrom();
                if (flows.length > 0) {
                    Address target = flows[0].getToAddress();
                    long t = target.getOffset();
                    String desc = "";
                    if (t >= 0x00000000L && t <= 0x00FFFFFFL) desc = " *** RAM-MODULE ***";
                    if (t >= 0xFF0A0000L && t <= 0xFF0B0000L) desc = " (local stub)";
                    instrStr = String.format("%s -> 0x%08X%s", instr.getMnemonicString(), t, desc);
                }
            } else {
                instrStr = "(no-instruction)";
            }

            String refs = "";
            Reference[] refsTo = getReferencesTo(a);
            if (refsTo.length > 0) {
                refs = " [" + refsTo.length + " refs: ";
                int n = Math.min(refsTo.length, 3);
                for (int i = 0; i < n; i++) {
                    if (i > 0) refs += ",";
                    refs += String.format("0x%08X", refsTo[i].getFromAddress().getOffset());
                }
                if (refsTo.length > n) refs += "...";
                refs += "]";
            }

            println(String.format("%-30s @ 0x%08X: %08X %08X %08X  %s%s",
                name, addr, w0, w1, w2, instrStr, refs));
        }

        // Now search strategically for BL to 0x000xxxxx range
        println("\n=== Searching for BL/RAM-module dispatch (limited) ===");
        int found = 0;
        int maxFind = 30;

        // Search in a specific range where kernel dispatch likely lives
        // The cstart/firmware init code copies firmware from ROM1 to RAM
        // Look around the firmware entry point region
        long searchStart = 0xFF0C0000L;  // firmware entry
        long searchEnd   = 0xFF0D0000L;  // 64KB window

        Address addr = toAddr(searchStart);
        while (addr.getOffset() < searchEnd && found < maxFind) {
            Instruction instr = getInstructionAt(addr);
            if (instr != null) {
                String mnem = instr.getMnemonicString();
                if (mnem.equals("bl") || mnem.equals("blx")) {
                    Address[] targets = instr.getFlows();
                    for (Address t : targets) {
                        long toff = t.getOffset();
                        if (toff >= 0x00100000L && toff <= 0x00FFFFFFL) {
                            println(String.format("  ROM+0x%06X: %s -> 0x%08X",
                                addr.getOffset() - 0xF8000000L, mnem, toff));
                            found++;
                        }
                    }
                }
                addr = addr.add(instr.getLength());
            } else {
                addr = addr.add(4);
            }
        }
        println(String.format("\nFound %d BL-to-RAM-module in firmware init region", found));
    }

    int getInt(byte[] b, int off) {
        return (b[off+3] & 0xFF) << 24 | (b[off+2] & 0xFF) << 16 |
               (b[off+1] & 0xFF) << 8  | (b[off] & 0xFF);
    }
}
