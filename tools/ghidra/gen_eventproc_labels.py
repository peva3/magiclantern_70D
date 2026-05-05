#!/usr/bin/env python3
"""
Apply eventproc function labels to Ghidra project.

Reads eventproc_map.csv and generates ApplyEventprocLabels.java
which labels each eventproc function at its ROM address.
"""
import csv

rom1_base = 0xF8000000

with open('tools/ghidra/eventproc_map.csv', 'r') as f:
    reader = csv.DictReader(f)
    entries = list(reader)

print(f"Generating labels for {len(entries)} eventproc functions...")

# Generate Java Ghidra script
java_lines = [
    "// Ghidra script to label eventproc functions by name",
    "//@category Canon70D",
    "",
    "import ghidra.app.script.GhidraScript;",
    "import ghidra.program.model.symbol.SourceType;",
    "import ghidra.program.model.symbol.SymbolTable;",
    "",
    "public class ApplyEventprocLabels extends GhidraScript {",
    "    @Override",
    "    protected void run() throws Exception {",
    "        SymbolTable sym = currentProgram.getSymbolTable();",
    "        int count = 0;",
    "        long[][] labels = {",
]

for entry in entries:
    name = entry['call_name']
    runtime_addr = int(entry['func_runtime_addr'], 16)
    rom_addr = runtime_addr - 0xFF000000 + rom1_base
    java_lines.append(f"            {{{rom_addr}L, \"{name}\"}},")

java_lines += [
    "        };",
    "",
    "        for (int i = 0; i < labels.length; i++) {",
    "            try {",
    "                sym.createLabel(toAddr(labels[i][0]), String.format(\"call_%s\", (Object)Long.toHexString(labels[i][1])), SourceType.USER_DEFINED);",
    "            } catch (Exception e) {}",
    "        }",
    "        println(\"Applied \" + count + \" eventproc labels\");",
    "    }",
    "}",
]

# Hmm, that approach won't work easily with strings in Java arrays
# Let me use a different approach - generate a simple Ghidra Jython script instead

jython_lines = [
    "# Jython script to label eventproc functions",
    "#@category Canon70D",
    "",
    "entries = [",
]
for entry in entries:
    name = entry['call_name']
    runtime_addr = int(entry['func_runtime_addr'], 16)
    rom_addr = runtime_addr - 0xFF000000 + rom1_base
    jython_lines.append(f"    ({rom_addr}, \"{name}\"),")

jython_lines += [
    "]",
    "",
    "count = 0",
    "for rom_addr, name in entries:",
    "    try:",
    "        addr = toAddr(rom_addr)",
    "        currentProgram.getSymbolTable().createLabel(addr, name,",
    "            ghidra.program.model.symbol.SourceType.USER_DEFINED)",
    "        count += 1",
    "    except:",
    "        pass",
    "print \"Applied %d eventproc labels\" % count",
]

script_path = 'tools/ghidra/ApplyEventprocLabels.jy'
with open(script_path, 'w') as f:
    f.write('\n'.join(jython_lines))

print(f"Generated: {script_path}")
print(f"Labels can be applied with:")
print(f"  /opt/ghidra/support/analyzeHeadless /app/70d 70D -process ROM1.BIN \\")
print(f"      -postScript {script_path} -scriptPath tools/ghidra -noanalysis")
