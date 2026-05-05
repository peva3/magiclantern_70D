// Ghidra script to find and extract the eventproc dispatch table
// call() uses: table walker at 0xf8144420, dispatch helper at 0xf8144340
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.lang.*;
import java.util.*;

public class FindEventprocTable extends GhidraScript {

    @Override
    protected void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        ReferenceManager rm = currentProgram.getReferenceManager();
        
        println("=== Eventproc Table Discovery ===");
        println("");
        
        // The table walker is at 0xF8144420
        long walkerAddr = 0xF8144420L;
        Address walker = toAddr(walkerAddr);
        
        println("Table walker function at 0xF8144420:");
        Function walkerFn = getFunctionAt(walker);
        if (walkerFn != null) {
            println("  Name: " + walkerFn.getName());
            println("  Body: " + walkerFn.getBody());
            
            // Find callers of the table walker
            println("");
            println("Callers of table walker:");
            ReferenceIterator refs = rm.getReferencesTo(walker);
            int c = 0;
            while (refs.hasNext() && c < 20) {
                Reference ref = refs.next();
                Address from = ref.getFromAddress();
                Function caller = getFunctionContaining(from);
                if (caller != null) {
                    println(String.format("  0x%X called from 0x%X (%s)", 
                        from.getOffset(), caller.getBody().getMinAddress().getOffset(), caller.getName()));
                    
                    // If the caller passes r0 as a table pointer, that pointer is the eventproc table
                    // Look at instructions before the call
                    Listing l = currentProgram.getListing();
                    InstructionIterator iter = l.getInstructions(caller.getBody(), true);
                    while (iter.hasNext()) {
                        Instruction inst = iter.next();
                        if (inst.getAddress().equals(from)) break; // reached the call
                        String instStr = inst.toString();
                        if (instStr.startsWith("ldr r0,") || instStr.startsWith("add r0,") || instStr.startsWith("mov r0,")) {
                            println("    Table setup: " + String.format("0x%X: %s", inst.getAddress().getOffset(), instStr));
                        }
                    }
                }
                c++;
            }
        }
        
        // Try to find the eventproc table by scanning for the table structure
        // The table is an array of {string_ptr, func_ptr} 8-byte entries, null-terminated
        println("");
        println("Scanning for eventproc tables (8-byte entries: str_ptr, func_ptr, ... null)...");
        int tablesFound = 0;
        
        for (long addr = 0xF8140000L; addr < 0xF8160000L; addr += 4) {
            try {
                // Read first entry: should be a valid string pointer
                byte[] b1 = new byte[4];
                mem.getBytes(toAddr(addr), b1);
                long strPtr = bytesToLong(b1);
                
                // String pointers are in ROM0 (F0000000-F07FFFFF) or ROM1 (F8000000-F8FFFFFF)
                if ((strPtr >= 0xF0000000L && strPtr < 0xF0800000L) || 
                    (strPtr >= 0xF8000000L && strPtr < 0xF9000000L)) {
                    
                    // Read second word: should be a function pointer
                    byte[] b2 = new byte[4];
                    mem.getBytes(toAddr(addr + 4), b2);
                    long funcPtr = bytesToLong(b2);
                    
                    if (funcPtr >= 0xFF000000L && funcPtr <= 0xFFFFFFFFL) {
                        // Verify: check there's a valid entry AFTER this one (index 1)
                        // and a null terminator somewhere in reasonable range
                        try {
                            byte[] b3 = new byte[4];
                            mem.getBytes(toAddr(addr + 8), b3);
                            long nextStr = bytesToLong(b3);
                            
                            // Also check the function pointer maps to a known function
                            long romFunc = funcPtr - 0xFF000000L + 0xF8000000L;
                            Function fn = null;
                            if (romFunc >= 0xF8000000L && romFunc < 0xF9000000L) {
                                fn = getFunctionAt(toAddr(romFunc));
                            }
                            
                            if (nextStr != 0 && fn != null) {
                                println(String.format("  TABLE START at 0x%X:", addr));
                                println(String.format("    Entry 0: str=0x%X func=0x%X [%s]", 
                                    strPtr, funcPtr, fn.getName()));
                                
                                // Read next few entries
                                for (int i = 1; i < 5; i++) {
                                    byte[] bn = new byte[4];
                                    mem.getBytes(toAddr(addr + i*8), bn);
                                    long ns = bytesToLong(bn);
                                    if (ns == 0) { println("    (end of table)"); break; }
                                    
                                    byte[] bf = new byte[4];
                                    mem.getBytes(toAddr(addr + i*8 + 4), bf);
                                    long nf = bytesToLong(bf);
                                    
                                    long romF = nf - 0xFF000000L + 0xF8000000L;
                                    Function nfn = null;
                                    if (romF >= 0xF8000000L && romF < 0xF9000000L) {
                                        nfn = getFunctionAt(toAddr(romF));
                                    }
                                    String fname = (nfn != null) ? nfn.getName() : "?";
                                    println(String.format("    Entry %d: str=0x%X func=0x%X [%s]", 
                                        i, ns, nf, fname));
                                }
                                
                                tablesFound++;
                                if (tablesFound >= 3) break;
                            }
                        } catch (Exception e) {}
                    }
                }
            } catch (Exception e) {}
        }
        
        if (tablesFound == 0) {
            println("  No eventproc tables found. The table may be in ROM0 or use different format.");
            
            // Try ROM0 space
            println("");
            println("Searching ROM0 space (0xF0000000-0xF0800000)...");
            for (long addr = 0xF0000000L; addr < 0xF0800000L; addr += 4) {
                // Scan for patterns
            }
        }
    }
    
    long bytesToLong(byte[] b) {
        return (b[0] & 0xFFL) | ((b[1] & 0xFFL) << 8) | 
               ((b[2] & 0xFFL) << 16) | ((b[3] & 0xFFL) << 24);
    }
}
