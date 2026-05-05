// Ghidra script to analyze boot sequence and interrupt handlers
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import java.util.*;

public class AnalyzeBoot extends GhidraScript {

    @Override
    protected void run() throws Exception {
        long cstartRuntime = 0xFF0C1BA8L;
        long cstartROM = 0xF80C1BA8L;
        
        println("=== Boot Analysis ===");
        
        // Find cstart function
        Address cstartAddr = toAddr(cstartROM);
        Function cstart = getFunctionAt(cstartAddr);
        if (cstart == null) {
            println("cstart function not found - checking for instructions");
            Instruction inst = getInstructionAt(cstartAddr);
            if (inst != null) {
                println("Found instruction, creating cstart function...");
                println("First instruction: " + inst.toString());
            }
        } else {
            println("cstart: " + cstart.getName() + " body=" + cstart.getBody());
            
            // List first 50 instructions of cstart
            println("\ncstart first 50 instructions:");
            Listing l = currentProgram.getListing();
            InstructionIterator iter = l.getInstructions(cstart.getBody(), true);
            int count = 0;
            while (iter.hasNext() && count < 50) {
                Instruction inst = iter.next();
                String s = String.format("  0x%X: %s", inst.getAddress().getOffset(), inst.toString());
                println(s);
                count++;
            }
        }
        
        // Find init_task function
        long initTaskROM = 0xF80C54CCL;
        Address initAddr = toAddr(initTaskROM);
        Function initTask = getFunctionAt(initAddr);
        if (initTask != null) {
            println("\ninit_task: " + initTask.getName() + " body=" + initTask.getBody());
        }
        
        // Look for DRYOS version string in ROM
        println("\nDRYOS version from hex analysis: 2.3, release #0051");
        
        // Analyze SWI handler - search for SWI-related code
        println("\nSearching for SWI handler dispatch...");
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        
        // Find functions that reference DRYOS strings
        println("\nFunctions referencing 'DRYOS PANIC':");
        // Search for the DRYOS PANIC string address
        // From our hex analysis, offset is 0xC1AAC in ROM
        long panicStrROM = 0xF8000000L + 0xC1AACL;
        Address panicAddr = toAddr(panicStrROM);
        ReferenceIterator panicRefs = refMgr.getReferencesTo(panicAddr);
        int panicCount = 0;
        while (panicRefs.hasNext() && panicCount < 10) {
            Reference ref = panicRefs.next();
            Function caller = getFunctionContaining(ref.getFromAddress());
            if (caller != null) {
                println(String.format("  called from 0x%X (%s)", ref.getFromAddress().getOffset(), caller.getName()));
                panicCount++;
            }
        }
        
        // Find the main dispatch function - task_create or similar
        long taskCreateROM = 0xF8000000L + 0x98CCL;  // task_create RAM offset
        Address tcAddr = toAddr(taskCreateROM);
        println("\ntask_create at: 0x" + Long.toHexString(taskCreateROM));
        println("  (runtime offset 0x98CC in DryOS module space)");
        
        // Check critical interrupt handler references
        println("\nFunctions near the reset vector area (0xF8FF0000):");
        Address vecArea = toAddr(0xF8FF0000L);
        Listing listing = currentProgram.getListing();
        InstructionIterator vecInsts = listing.getInstructions(vecArea, true);
        int vc = 0;
        for (Instruction inst : vecInsts) {
            if (vc > 20) break;
            println(String.format("  0x%X: %s", inst.getAddress().getOffset(), inst.toString()));
            vc++;
        }
    }
}
