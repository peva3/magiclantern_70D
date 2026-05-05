/*Ghidra Script for Canon 70D ROM1.BIN symbol labeling
 * Reads all_symbols.csv and creates labels
 *@category Canon70D
 * Run: analyzeHeadless <project_dir> <project_name> -process ROM1.BIN -postScript ApplyLabels.java -scriptPath <script_dir>
 */

import java.io.*;
import java.util.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.SymbolTable;

public class ApplyLabels extends GhidraScript {

    @Override
    protected void run() throws Exception {
        SymbolTable sym = currentProgram.getSymbolTable();

        // Look for CSV in several locations
        String[] paths = {
            "/app/70d/tools/ghidra/all_symbols.csv"
        };

        File csvFile = null;
        for (String p : paths) {
            File f = new File(p);
            if (f.exists()) {
                csvFile = f;
                break;
            }
        }

        if (csvFile == null) {
            println("ERROR: Could not find all_symbols.csv");
            return;
        }

        int count = 0;
        BufferedReader br = new BufferedReader(new FileReader(csvFile));
        String header = br.readLine(); // skip header

        String line;
        while ((line = br.readLine()) != null) {
            String[] parts = line.split(",");
            if (parts.length < 2) continue;
            String name = parts[0].trim();
                String addrStr = parts[1].trim();
            if (addrStr.startsWith("0x") || addrStr.startsWith("0X")) {
                try {
                    long addr = Long.parseLong(addrStr.substring(2), 16);
                    /* Map runtime addresses to ROM1 offsets */
                    /* Firmware copies from ROM1 (0xF8000000) to runtime (0xFF000000) */
                    if (addr >= 0xFF000000L && addr <= 0xFFFFFFFFL) {
                        long romAddr = addr - 0xFF000000L + 0xF8000000L;
                        /* Only label if within ROM1 range */
                        if (romAddr >= 0xF8000000L && romAddr < 0xF9000000L) {
                            sym.createLabel(toAddr(romAddr), name, SourceType.USER_DEFINED);
                            count++;
                        }
                    }
                } catch (Exception e) {
                    // skip parse errors
                }
            }
        }
        br.close();

        println("Applied " + count + " symbols from " + csvFile.getName());
    }
}
