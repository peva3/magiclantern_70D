// Ghidra script to report project statistics
//@category Canon70D

import ghidra.app.script.GhidraScript;

public class ReportStats extends GhidraScript {
    @Override
    protected void run() throws Exception {
        long funcCount = currentProgram.getFunctionManager().getFunctionCount();
        long instrCount = currentProgram.getListing().getNumInstructions();
        long definedData = currentProgram.getListing().getNumDefinedData();
        long minAddr = currentProgram.getMinAddress().getOffset();
        long maxAddr = currentProgram.getMaxAddress().getOffset();
        long symCount = currentProgram.getSymbolTable().getNumSymbols();

        println("=== 70D ROM1 Ghidra Analysis Results ===");
        println("Functions: " + funcCount);
        println("Instructions: " + instrCount);
        println("Defined Data: " + definedData);
        println(String.format("Address Range: 0x%X - 0x%X", minAddr, maxAddr));
        println("Symbols: " + symCount);
        println("Memory Blocks: " + currentProgram.getMemory().getBlocks().length);
        println("========================================");
    }
}
