// Ghidra script to generate function cluster report for known NSTUBs
// Shows how many functions call each known stub
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import java.util.*;

public class ClusterReport extends GhidraScript {

    @Override
    protected void run() throws Exception {
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        SymbolTable symTable = currentProgram.getSymbolTable();
        
        println("=== Function Cluster Report ===");
        println("How many callers each NSTUB has:");
        println("");
        
        // Get all our user-defined symbols
        SymbolIterator symIter = symTable.getSymbolIterator();
        List<Symbol> ourSyms = new ArrayList<>();
        while (symIter.hasNext()) {
            Symbol s = symIter.next();
            if (s.getSource().equals(SourceType.USER_DEFINED)) {
                ourSyms.add(s);
            }
        }
        println("User-defined symbols: " + ourSyms.size() + "\n");
        
        // For each symbol, count callers
        Collections.sort(ourSyms, (a, b) -> {
            return Long.compare(a.getAddress().getOffset(), b.getAddress().getOffset());
        });
        
        // Group by function size/caller pattern
        for (Symbol s : ourSyms) {
            Address addr = s.getAddress();
            int callerCount = 0;
            ReferenceIterator refs = refMgr.getReferencesTo(addr);
            while (refs.hasNext()) {
                refs.next();
                callerCount++;
            }
            
            // Get function size if applicable
            String sizeStr = "";
            Function func = getFunctionAt(addr);
            if (func != null) {
                long sz = func.getBody().getNumAddresses();
                sizeStr = String.format(" [%d addr]", sz);
            }
            
            println(String.format("  %4d callers  0x%X %s%s", callerCount, addr.getOffset(), s.getName(), sizeStr));
        }
    }
}
