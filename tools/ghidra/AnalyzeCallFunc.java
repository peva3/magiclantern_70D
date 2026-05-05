// Ghidra script to analyze the call() function and eventproc dispatch table
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import java.util.*;

public class AnalyzeCallFunc extends GhidraScript {

    @Override
    protected void run() throws Exception {
        long callAddr = 0xF814439CL;
        Address callFn = toAddr(callAddr);
        
        println("=== call() Function Analysis ===");
        
        Function func = getFunctionAt(callFn);
        if (func == null) {
            Instruction inst = getInstructionAt(callFn);
            if (inst != null) {
                println("Found instruction at call() address, creating function");
                func = createFunction(callFn, "call_dispatch");
            } else {
                println("No instruction at call() address");
                return;
            }
        }
        
        println("Function: " + func.getName());
        println("Body: " + func.getBody());
        
        // Get callers of call()
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        ReferenceIterator refs = refMgr.getReferencesTo(callFn);
        int callerCount = 0;
        println("\nCallers of call() (first 50):");
        while (refs.hasNext() && callerCount < 50) {
            Reference ref = refs.next();
            Address fromAddr = ref.getFromAddress();
            Function caller = getFunctionContaining(fromAddr);
            String callerName = (caller != null) ? caller.getName() : "?";
            println(String.format("  0x%X (%s)", fromAddr.getOffset(), callerName));
            callerCount++;
        }
        println("Total unique callers found: " + callerCount);
        
        // Examine instructions in call()
        println("\nInstructions in call():");
        Listing l = currentProgram.getListing();
        InstructionIterator iter = l.getInstructions(func.getBody(), true);
        int count = 0;
        while (iter.hasNext() && count < 200) {
            Instruction inst = iter.next();
            String instRefs = "";
            Reference[] irefs = inst.getReferencesFrom();
            if (irefs.length > 0) {
                StringBuilder sb = new StringBuilder(" ->");
                for (Reference r : irefs) {
                    sb.append(String.format(" 0x%X", r.getToAddress().getOffset()));
                }
                instRefs = sb.toString();
            }
            println(String.format("  0x%X: %s%s", inst.getAddress().getOffset(), inst.toString(), instRefs));
            count++;
        }
        println("Total instructions: " + count);
        
        // Check for potential string tables referenced nearby
        println("\nSearching for eventproc string table...");
        // Look for references from call() that point to data
        for (Reference ref : refMgr.getReferencesFrom(callFn)) {
            Address toAddr = ref.getToAddress();
            long toOff = toAddr.getOffset();
            if (toOff >= 0xF8000000L && toOff < 0xF9000000L) {
                Data data = getDataAt(toAddr);
                if (data != null) {
                    println(String.format("  Data at 0x%X: %s", toOff, data.getDefaultValueRepresentation()));
                }
            }
        }
    }
}
