// StringRefLabeler: Labels unnamed functions by string references, NSTUB calls, and wrapper patterns
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.data.*;
import ghidra.util.task.TaskMonitor;
import java.util.*;

public class StringRefLabeler extends GhidraScript {

    @Override
    protected void run() throws Exception {
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        FunctionManager fMgr = currentProgram.getFunctionManager();
        SymbolTable symTable = currentProgram.getSymbolTable();
        Listing listing = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();

        println("=== StringRefLabeler: Intelligent Function Labeling ===");

        // Phase 0: Collect all function addresses and names
        println("\nPhase 0: Indexing all functions...");
        Map<Address, Function> allFunctions = new HashMap<>();
        Set<Address> unnamedFuncs = new HashSet<>();
        Set<Address> namedFuncs = new HashSet<>();
        for (Function f : fMgr.getFunctions(true)) {
            Address entry = f.getEntryPoint();
            allFunctions.put(entry, f);
            if (f.getName().startsWith("FUN_")) {
                unnamedFuncs.add(entry);
            } else {
                namedFuncs.add(entry);
            }
        }
        println("  Total: " + allFunctions.size() + ", Unnamed (FUN_*): " + unnamedFuncs.size() + ", Named: " + namedFuncs.size());

        // Collect all non-default named symbols (seeds)
        Set<Address> seedAddrs = new HashSet<>();
        Map<Address, String> seedNames = new HashMap<>();
        SymbolIterator symIter = symTable.getSymbolIterator();
        while (symIter.hasNext()) {
            Symbol s = symIter.next();
            if (s.getSource() == SourceType.USER_DEFINED || s.getSource() == SourceType.ANALYSIS) {
                if (!s.getName().startsWith("FUN_")) {
                    seedAddrs.add(s.getAddress());
                    seedNames.put(s.getAddress(), s.getName());
                }
            }
        }
        println("  Seed symbols (non-FUN_*): " + seedAddrs.size());

        // Phase 1: Label functions by called NSTUBs
        println("\nPhase 1: Labeling unnamed functions that call known NSTUBs...");
        int labeledByCall = 0;
        for (Address fnAddr : unnamedFuncs) {
            Function fn = allFunctions.get(fnAddr);
            if (fn == null) continue;
            String currentName = fn.getName();

            Set<String> calledSeedNames = new LinkedHashSet<>();
            InstructionIterator insts = listing.getInstructions(fn.getBody(), true);
            while (insts.hasNext()) {
                Instruction inst = insts.next();
                for (Reference r : inst.getReferencesFrom()) {
                    Address target = r.getToAddress();
                    if (seedAddrs.contains(target)) {
                        calledSeedNames.add(seedNames.get(target));
                    } else if (allFunctions.containsKey(target) && !allFunctions.get(target).getName().startsWith("FUN_")) {
                        calledSeedNames.add(allFunctions.get(target).getName());
                    }
                }
            }

            if (calledSeedNames.size() >= 1) {
                String newName;
                if (calledSeedNames.size() == 1) {
                    newName = "call_" + calledSeedNames.iterator().next();
                } else {
                    // Pick the most descriptive (longest) name from the first few callees
                    String best = "";
                    for (String n : calledSeedNames) {
                        if (n.length() > best.length()) best = n;
                    }
                    newName = "calls_" + best;
                }
                newName = sanitize(newName);
                try {
                    fn.setName(newName, SourceType.ANALYSIS);
                    labeledByCall++;
                } catch (Exception e) {
                    // name conflict
                }
            }
        }
        println("  Labeled " + labeledByCall + " functions by NSTUB calls");

        // Refresh unnamed set
        unnamedFuncs.clear();
        for (Function f : fMgr.getFunctions(true)) {
            if (f.getName().startsWith("FUN_")) {
                unnamedFuncs.add(f.getEntryPoint());
            }
        }

        // Phase 2: Label functions by unique string references in their literal pool
        println("\nPhase 2: Labeling by string references...");
        int labeledByString = 0;

        // Build a map of string address -> string content for all ROM strings
        Map<Address, String> stringMap = new HashMap<>();
        for (MemoryBlock block : mem.getBlocks()) {
            if (!block.isInitialized()) continue;
            Address start = block.getStart();
            Address end = block.getEnd();
            long size = block.getSize();
            if (size > 0x200000) size = 0x200000; // limit scan to 2MB per block for speed
            try {
                byte[] data = new byte[(int)size];
                mem.getBytes(start, data);
                int i = 0;
                while (i < data.length) {
                    // Find printable ASCII strings of length >= 6
                    if (data[i] >= 0x20 && data[i] <= 0x7e) {
                        int j = i;
                        while (j < data.length && data[j] >= 0x20 && data[j] <= 0x7e) j++;
                        int len = j - i;
                        if (len >= 6 && len <= 80) {
                            String s = new String(data, i, len);
                            // Filter useful-looking strings
                            if (s.contains("_") || s.contains("/") || s.contains(".") ||
                                s.matches(".*[A-Z].*[a-z].*") || s.matches(".*[0-9].*")) {
                                Address strAddr = start.add(i);
                                stringMap.put(strAddr, s);
                            }
                        }
                        i = j;
                    } else {
                        i++;
                    }
                }
            } catch (Exception e) {
                println("  Warning: could not scan block " + block.getName() + ": " + e.getMessage());
            }
        }
        println("  Found " + stringMap.size() + " potential string references in ROM");

        // For each unnamed function, find strings it references
        for (Address fnAddr : unnamedFuncs) {
            Function fn = allFunctions.get(fnAddr);
            if (fn == null) continue;

            Set<String> referencedStrings = new LinkedHashSet<>();
            InstructionIterator insts = listing.getInstructions(fn.getBody(), true);
            while (insts.hasNext()) {
                Instruction inst = insts.next();
                for (Reference r : inst.getReferencesFrom()) {
                    Address target = r.getToAddress();
                    if (target == null) continue;
                    // Check if target is within any memory block with data
                    // Look for direct string references
                    if (stringMap.containsKey(target)) {
                        referencedStrings.add(stringMap.get(target));
                    }
                    // Also check if this address is in a data block near a string
                    for (Map.Entry<Address, String> entry : stringMap.entrySet()) {
                        long diff = target.getOffset() - entry.getKey().getOffset();
                        if (diff >= 0 && diff < 100 && !entry.getValue().isEmpty()) {
                            referencedStrings.add(entry.getValue());
                            break;
                        }
                    }
                }
            }

            if (referencedStrings.size() == 1) {
                String str = referencedStrings.iterator().next();
                String label = strToLabel(str);
                if (label != null && label.length() > 2) {
                    String newName = "str_" + label;
                    newName = sanitize(newName);
                    if (newName.length() > 80) newName = newName.substring(0, 80);
                    try {
                        fn.setName(newName, SourceType.ANALYSIS);
                        labeledByString++;
                    } catch (Exception e) {}
                }
            }
        }
        println("  Labeled " + labeledByString + " functions by string reference");

        // Refresh unnamed set
        unnamedFuncs.clear();
        for (Function f : fMgr.getFunctions(true)) {
            if (f.getName().startsWith("FUN_")) {
                unnamedFuncs.add(f.getEntryPoint());
            }
        }

        // Phase 3: Label wrapper/thunk functions
        println("\nPhase 3: Labeling wrapper/thunk functions...");
        int wrappers = 0;
        for (Address fnAddr : unnamedFuncs) {
            Function fn = allFunctions.get(fnAddr);
            if (fn == null) continue;

            // Count instructions and callees
            int instCount = 0;
            Set<Function> callees = new LinkedHashSet<>();
            InstructionIterator insts = listing.getInstructions(fn.getBody(), true);
            while (insts.hasNext()) {
                Instruction inst = insts.next();
                instCount++;
                for (Reference r : inst.getReferencesFrom()) {
                    Function callee = fMgr.getFunctionAt(r.getToAddress());
                    if (callee != null && !callee.getName().startsWith("FUN_")) {
                        callees.add(callee);
                    }
                }
            }

            // Wrapper: <= 8 instructions calling exactly one named function
            if (instCount <= 8 && callees.size() == 1) {
                Function callee = callees.iterator().next();
                String wrapName = "wrap_" + callee.getName();
                wrapName = sanitize(wrapName);
                if (wrapName.length() > 80) wrapName = wrapName.substring(0, 80);
                try {
                    fn.setName(wrapName, SourceType.ANALYSIS);
                    wrappers++;
                } catch (Exception e) {}
            }
        }
        println("  Labeled " + wrappers + " wrapper functions");

        // Phase 4: Chain propagation — label callers of newly named functions
        println("\nPhase 4: Chain propagation (2nd pass)...");
        int chainLabeled = 0;
        // Collect newly named functions
        namedFuncs.clear();
        for (Function f : fMgr.getFunctions(true)) {
            if (!f.getName().startsWith("FUN_")) {
                namedFuncs.add(f.getEntryPoint());
            }
        }
        for (Address fnAddr : unnamedFuncs) {
            Function fn = allFunctions.get(fnAddr);
            if (fn == null) continue;
            // Check if this function calls exactly one of the newly named functions
            Set<Function> callees = new LinkedHashSet<>();
            InstructionIterator insts = listing.getInstructions(fn.getBody(), true);
            while (insts.hasNext()) {
                Instruction inst = insts.next();
                for (Reference r : inst.getReferencesFrom()) {
                    Function callee = fMgr.getFunctionAt(r.getToAddress());
                    if (callee != null && namedFuncs.contains(r.getToAddress())) {
                        callees.add(callee);
                    }
                }
            }
            if (callees.size() == 1) {
                Function callee = callees.iterator().next();
                String chainName = "via_" + callee.getName();
                chainName = sanitize(chainName);
                if (chainName.length() > 80) chainName = chainName.substring(0, 80);
                try {
                    fn.setName(chainName, SourceType.ANALYSIS);
                    chainLabeled++;
                } catch (Exception e) {}
            }
        }
        println("  Labeled " + chainLabeled + " via chain propagation");

        // Final summary
        int totalCustom = 0;
        symIter = symTable.getSymbolIterator();
        while (symIter.hasNext()) {
            Symbol s = symIter.next();
            if (s.getSource() == SourceType.USER_DEFINED || s.getSource() == SourceType.ANALYSIS) {
                if (!s.getName().startsWith("FUN_")) {
                    totalCustom++;
                }
            }
        }
        println("\n=== Final Symbol Summary ===");
        println("  Total non-FUN_* symbols: " + totalCustom);
        println("  Total all symbols: " + symTable.getNumSymbols());
        println("  Newly labeled this run: " + (labeledByCall + labeledByString + wrappers + chainLabeled));
    }

    String sanitize(String name) {
        name = name.replaceAll("[^a-zA-Z0-9_]", "_");
        name = name.replaceAll("__+", "_");
        name = name.replaceAll("^_|_$", "");
        return name;
    }

    String strToLabel(String str) {
        // Extract a meaningful label from a string reference
        // Handle various patterns
        str = str.trim();
        if (str.isEmpty()) return null;

        // Take the last path component for file paths
        if (str.contains("/")) {
            str = str.substring(str.lastIndexOf('/') + 1);
        }
        if (str.contains("\\")) {
            str = str.substring(str.lastIndexOf('\\') + 1);
        }

        // Remove file extension
        if (str.contains(".")) {
            str = str.substring(0, str.lastIndexOf('.'));
        }

        // Remove non-alphanumeric prefix
        str = str.replaceAll("^[^a-zA-Z0-9_]+", "");

        // Truncate reasonable length
        if (str.length() > 40) {
            str = str.substring(0, 40);
        }

        if (str.isEmpty()) return null;
        return str;
    }
}
