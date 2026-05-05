// Ghidra script for automated function labeling via call-graph propagation
// Uses 98 known NSTUBs as seeds, labels nearby functions by proximity and string refs
//@category Canon70D

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.mem.*;
import java.util.*;

public class AutoLabelFunctions extends GhidraScript {

    // Subsystem names mapped from known NSTUBs
    static final String[][] SUBSYSTEMS = {
        {"FIO_", "FIO"},
        {"EDmac", "EDMAC"},
        {"prop_", "PROP"},
        {"shamem", "ENGIO"},
        {"EngDrvOut", "ENGIO"},
        {"engio_write", "ENGIO"},
        {"StartEDmac", "EDMAC"},
        {"StartASIF", "AUDIO"},
        {"SetAudio", "AUDIO"},
        {"PowerMic", "AUDIO"},
        {"PowerAudio", "AUDIO"},
        {"sounddev", "AUDIO"},
        {"SoundDev", "AUDIO"},
        {"audio_ic", "AUDIO"},
        {"mvr", "MVR"},
        {"ptpip_", "PTPIP"},
        {"socket_", "SOCKET"},
        {"call", "EVENTPROC"},
        {"dialog_", "GUI"},
        {"GUI_", "GUI"},
        {"LiveView", "LV"},
        {"gui_", "GUI"},
        {"ErrCard", "LV"},
        {"ErrFor", "GUI"},
        {"ShootOlc", "SHOOT"},
        {"PlayMovie", "PLAY"},
        {"PlayMain", "PLAY"},
        {"MirrorDisplay", "DISPLAY"},
        {"NormalDisplay", "DISPLAY"},
        {"ReverseDisplay", "DISPLAY"},
        {"PACK16", "IMAGEPATH"},
        {"ProcessPath", "IMAGEPATH"},
        {"fsuDecode", "SDCARD"},
        {"GetCFn", "CFN"},
        {"SetCFn", "CFN"},
        {"pte", "PTP"},
        {"ptpProp", "PTP"},
        {"ptp_register", "PTP"},
        {"SRM_", "SRM"},
        {"Allocate", "MEMORY"},
        {"Free", "MEMORY"},
        {"malloc", "MEMORY"},
        {"free", "MEMORY"},
        {"CreateResLock", "ENGINE"},
        {"LockEngine", "ENGINE"},
        {"UnLockEngine", "ENGINE"},
        {"msleep", "DRYOS"},
        {"task_", "DRYOS"},
        {"semaphore", "DRYOS"},
        {"msg_queue", "DRYOS"},
        {"Cancel", "TIMER"},
        {"SetTimer", "TIMER"},
        {"SetHPTimer", "TIMER"},
        {"SetGUI", "GUI"},
        {"LoadCalendar", "RTC"},
        {"SetSamplingRate", "AUDIO"},
        {"mvrSet", "MVR"},
        {"mvrFix", "MVR"},
    };

    static class SymbolInfo {
        String name;
        Address addr;
        String subsystem;
        boolean isLabeled;

        SymbolInfo(String n, Address a, String s) {
            name = n; addr = a; subsystem = s; isLabeled = true;
        }
    }

    @Override
    protected void run() throws Exception {
        ReferenceManager refMgr = currentProgram.getReferenceManager();
        FunctionManager fMgr = currentProgram.getFunctionManager();
        SymbolTable symTable = currentProgram.getSymbolTable();
        Listing l = currentProgram.getListing();
        Memory mem = currentProgram.getMemory();

        println("=== Automated Function Labeling ===");
        println("Phase 1: Collect seed symbols...");

        // Collect all user-defined (known) symbols as seeds
        Map<String, SymbolInfo> seedSyms = new HashMap<>();
        SymbolIterator symIter = symTable.getSymbolIterator();
        while (symIter.hasNext()) {
            Symbol s = symIter.next();
            if (s.getSource() == SourceType.USER_DEFINED) {
                String name = s.getName();
                Address addr = s.getAddress();
                // Determine subsystem
                String subsys = "UNKNOWN";
                for (String[] mapping : SUBSYSTEMS) {
                    if (name.contains(mapping[0])) {
                        subsys = mapping[1];
                        break;
                    }
                }
                seedSyms.put(name, new SymbolInfo(name, addr, subsys));
            }
        }
        println("  Found " + seedSyms.size() + " seed symbols");

        // Phase 2: Find callers of each seed symbol and label them
        println("\nPhase 2: Labeling direct callers of known functions...");
        int labeled = 0;
        for (SymbolInfo si : seedSyms.values()) {
            ReferenceIterator refs = refMgr.getReferencesTo(si.addr);
            while (refs.hasNext()) {
                Reference ref = refs.next();
                Address callerAddr = ref.getFromAddress();
                Function callerFn = fMgr.getFunctionContaining(callerAddr);
                if (callerFn == null) continue;

                String callerName = callerFn.getName();
                if (callerName.startsWith("FUN_")) {
                    // Generate a meaningful name: subsystem_verb_from_seed
                    String newName = si.subsystem.toLowerCase() + "_from_" + si.name.toLowerCase();
                    if (newName.length() > 80) {
                        newName = newName.substring(0, 80);
                    }
                    try {
                        callerFn.setName(newName, SourceType.ANALYSIS);
                        labeled++;
                    } catch (Exception e) {
                        // name may conflict
                    }
                }
            }
        }
        println("  Labeled " + labeled + " caller functions");

        // Phase 3: Find functions that reference known ROM strings
        println("\nPhase 3: Labeling by string reference...");
        List<String> knownStrings = Arrays.asList(
            "DRYOS version", "DRYOS PANIC", "EnableBootDisk", "EnableFirmware",
            "./BootInfo", "FIO_", "edmac", "prop_", "call", "ptpip",
            "socket_", "dialog_", "EngDrvOut", "shamem_read"
        );

        int stringLabeled = 0;
        MemoryBlock[] blocks = mem.getBlocks();
        for (MemoryBlock block : blocks) {
            if (!block.isExecute()) continue;
            Address blockStart = block.getStart();
            Address blockEnd = block.getEnd();
            try {
                // Search for string references in memory
                byte[] blockData = new byte[(int)block.getSize()];
                mem.getBytes(blockStart, blockData);
                
                for (String s : knownStrings) {
                    byte[] pattern = s.getBytes();
                    int pos = 0;
                    while (true) {
                        int found = indexOf(blockData, pattern, pos);
                        if (found < 0) break;
                        Address strAddr = blockStart.add(found);
                        // Check if any function references this address
                        ReferenceIterator stringRefs = refMgr.getReferencesTo(strAddr);
                        while (stringRefs.hasNext()) {
                            Reference sr = stringRefs.next();
                            Function sf = fMgr.getFunctionContaining(sr.getFromAddress());
                            if (sf != null && sf.getName().startsWith("FUN_")) {
                                String baseName = s.replace("./", "").replace("-", "_")
                                    .replace(".", "_").replace("/", "_");
                                String sfName = "ref_" + baseName;
                                try {
                                    sf.setName(sfName, SourceType.ANALYSIS);
                                    stringLabeled++;
                                } catch (Exception e) {}
                            }
                        }
                        pos = found + 1;
                    }
                }
            } catch (Exception e) {
                // memory access exception
            }
        }
        println("  Labeled " + stringLabeled + " by string reference");

        // Phase 4: Label wrapper functions (very short functions that call only one thing)
        println("\nPhase 4: Labeling wrapper/thunk functions...");
        int wrappers = 0;
        for (Function func : fMgr.getFunctions(true)) {
            if (!func.getName().startsWith("FUN_")) continue;
            long numInst = func.getBody().getNumAddresses();
            if (numInst <= 5) {  // very small functions
                // Check what it calls
                Set<Function> callees = new HashSet<>();
                InstructionIterator insts = l.getInstructions(func.getBody(), true);
                while (insts.hasNext()) {
                    Instruction inst = insts.next();
                    for (Reference r : inst.getReferencesFrom()) {
                        Function callee = fMgr.getFunctionAt(r.getToAddress());
                        if (callee != null && !callee.getName().startsWith("FUN_")) {
                            callees.add(callee);
                        }
                    }
                }
                if (callees.size() == 1) {
                    Function callee = callees.iterator().next();
                    String wrapName = "wrap_" + callee.getName();
                    try {
                        func.setName(wrapName, SourceType.ANALYSIS);
                        wrappers++;
                    } catch (Exception e) {}
                }
            }
        }
        println("  Labeled " + wrappers + " wrapper functions");

        // Final summary
        int totalUser = 0;
        int totalAnalysis = 0;
        symIter = symTable.getSymbolIterator();
        while (symIter.hasNext()) {
            Symbol s = symIter.next();
            if (s.getSource() == SourceType.USER_DEFINED) totalUser++;
            if (s.getSource() == SourceType.ANALYSIS) totalAnalysis++;
        }
        println("\n=== Final Symbol Count ===");
        println("  User-defined: " + totalUser);
        println("  Analysis-labeled: " + totalAnalysis);
        println("  Total non-default symbols: " + (totalUser + totalAnalysis));
        println("  Total all symbols: " + symTable.getNumSymbols());
    }

    int indexOf(byte[] data, byte[] pattern, int start) {
        outer: for (int i = start; i <= data.length - pattern.length; i++) {
            for (int j = 0; j < pattern.length; j++) {
                if (data[i + j] != pattern[j]) continue outer;
            }
            return i;
        }
        return -1;
    }
}
