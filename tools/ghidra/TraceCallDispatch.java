// Ghidra script to deeply analyze the call() eventproc dispatch
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.lang.*;
import java.util.*;

public class TraceCallDispatch extends GhidraScript {

    @Override
    protected void run() throws Exception {
        println("=== Deep analysis of call() eventproc dispatch ===");
        println("");
        
        long callAddr = 0xF814439CL;
        Address callFn = toAddr(callAddr);
        Memory mem = currentProgram.getMemory();
        
        Function func = getFunctionAt(callFn);
        if (func == null) {
            println("ERROR: call() not found, creating...");
            func = createFunction(callFn, "call_dispatch");
        }
        
        println("Body: " + func.getBody());
        println("");
        println("Instructions:");
        println("");
        
        Listing l = currentProgram.getListing();
        InstructionIterator iter = l.getInstructions(func.getBody(), true);
        while (iter.hasNext()) {
            Instruction inst = iter.next();
            String s = String.format("  0x%X: %s", inst.getAddress().getOffset(), inst.toString());
            Reference[] refs = inst.getReferencesFrom();
            if (refs.length > 0) {
                s += "  ; refs:";
                for (Reference r : refs) {
                    Address toAddr = r.getToAddress();
                    Symbol sym = getSymbolAt(toAddr);
                    if (sym != null) {
                        s += String.format(" %s", sym.getName());
                    } else {
                        s += String.format(" 0x%X", toAddr.getOffset());
                    }
                }
            }
            println(s);
        }
        
        // Print instructions right after call() (nearby functions)
        println("");
        println("Instructions after call() (potential helper funcs):");
        for (long addr = 0xF81443ECL; addr < 0xF8144700L; addr += 4) {
            Instruction inst = getInstructionAt(toAddr(addr));
            if (inst != null) {
                String s = String.format("  0x%X: %s", addr, inst.toString());
                Function containing = getFunctionContaining(toAddr(addr));
                if (containing != null && containing.getBody().getMinAddress().getOffset() == addr) {
                    s += String.format("  [START of %s]", containing.getName());
                }
                println(s);
            }
        }
        
        // Read function at nearby addresses to find eventproc table walker
        println("");
        println("Searching for string table references near call()...");
        
        // call() might call strcmp or a hash function. Find what's at nearby addresses
        ReferenceManager rm = currentProgram.getReferenceManager();
        int found = 0;
        
        // Scan a larger range for function pointer table patterns
        for (long addr = 0xF8142000L; addr < 0xF8150000L; addr += 4) {
            try {
                byte[] bytes = new byte[4];
                mem.getBytes(toAddr(addr), bytes);
                int val = (bytes[0] & 0xFF) | ((bytes[1] & 0xFF) << 8) | 
                         ((bytes[2] & 0xFF) << 16) | ((bytes[3] & 0xFF) << 24);
                long uval = val & 0xFFFFFFFFL;
                
                if (uval >= 0xFF000000L && uval <= 0xFFFFFFFFL) {
                    // Potential function pointer (runtime address)
                    // Map to ROM offset
                    long romOff = uval - 0xFF000000L + 0xF8000000L;
                    if (romOff >= 0xF8000000L && romOff < 0xF9000000L) {
                        Function fn = getFunctionAt(toAddr(romOff));
                        if (fn != null) {
                            // Check for string pointer at preceding word
                            try {
                                byte[] prevBytes = new byte[4];
                                mem.getBytes(toAddr(addr - 4), prevBytes);
                                int prevVal = (prevBytes[0] & 0xFF) | ((prevBytes[1] & 0xFF) << 8) | 
                                             ((prevBytes[2] & 0xFF) << 16) | ((prevBytes[3] & 0xFF) << 24);
                                long prevUval = prevVal & 0xFFFFFFFFL;
                                
                                // String pointers in ROM0 space (0xF0000000-0xF07FFFFF) or ROM1 (0xF8000000-0xF8FFFFFF)
                                boolean isRomStr = (prevUval >= 0xF0000000L && prevUval < 0xF0800000L) ||
                                                  (prevUval >= 0xF8000000L && prevUval < 0xF9000000L);
                                if (isRomStr) {
                                    println(String.format("  TABLE at 0x%X: str_ptr=0x%X func_ptr=0x%X [%s]", 
                                        addr - 4, prevUval, uval, fn.getName()));
                                    found++;
                                    if (found >= 20) break;
                                }
                            } catch (Exception e) {}
                        }
                    }
                }
            } catch (Exception e) {
                // memory read failure
            }
        }
        if (found == 0) {
            println("  No eventproc table found in 0xF8142000-0xF8150000");
            println("  The table may be in ROM0 (assets) or use a different format");
        }
    }
}
