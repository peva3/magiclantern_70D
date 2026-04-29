#!/usr/bin/env python3
"""
hw_test log analyzer for QEMU 70D
Parses DebugMsg logs and extracts test results
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

def parse_hw_test_log(log_file):
    """Parse hw_test results from DebugMsg log file"""
    results = {
        'pass': [],
        'fail': [],
        'skip': [],
        'info': [],
        'errors': []
    }
    
    if not Path(log_file).exists():
        print(f"Error: {log_file} not found")
        return results
    
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Remove ANSI color codes
    content = re.sub(r'\x1b\[[0-9;]*[mK]', '', content)
    
    # Find test results
    for line in content.split('\n'):
        # PASS
        if '[HW_TEST]' in line and ': PASS' in line:
            match = re.search(r'\[HW_TEST\] ([^:]+): PASS', line)
            if match:
                results['pass'].append(match.group(1).strip())
        
        # FAIL
        if '[HW_TEST]' in line and ': FAIL' in line:
            match = re.search(r'\[HW_TEST\] ([^:]+): FAIL', line)
            if match:
                results['fail'].append(match.group(1).strip())
        
        # SKIP
        if '[HW_TEST]' in line and ': SKIP' in line:
            match = re.search(r'\[HW_TEST\] ([^:]+): SKIP', line)
            if match:
                results['skip'].append(match.group(1).strip())
        
        # INFO messages
        if '[HW_TEST]' in line and ('OK' in line or 'bytes' in line or 'ms' in line):
            if ': PASS' not in line and ': FAIL' not in line and ': SKIP' not in line:
                results['info'].append(line.strip())
    
    return results

def print_summary(results):
    """Print test summary"""
    print("\n" + "="*60)
    print("HW_TEST Results Summary")
    print("="*60)
    
    print(f"\n✅ PASS: {len(results['pass'])}")
    for test in results['pass']:
        print(f"   - {test}")
    
    if results['fail']:
        print(f"\n❌ FAIL: {len(results['fail'])}")
        for test in results['fail']:
            print(f"   - {test}")
    
    if results['skip']:
        print(f"\n⏭️  SKIP: {len(results['skip'])}")
        for test in results['skip']:
            print(f"   - {test}")
    
    if results['info']:
        print(f"\nℹ️  INFO: {len(results['info'])}")
        for info in results['info'][:10]:  # Show first 10
            print(f"   - {info}")
        if len(results['info']) > 10:
            print(f"   ... and {len(results['info'])-10} more")
    
    print("\n" + "="*60)
    total = len(results['pass']) + len(results['fail']) + len(results['skip'])
    print(f"Total: {total} tests ({len(results['pass'])}/{total} passed)")
    print("="*60 + "\n")

def main():
    # Find latest log file
    log_dir = Path('/app/70d/logs')
    if not log_dir.exists():
        print("Error: logs directory not found")
        sys.exit(1)
    
    log_files = sorted(log_dir.glob('debugmsg_70D_*.log'), reverse=True)
    if not log_files:
        print("Error: No DebugMsg logs found")
        sys.exit(1)
    
    latest_log = str(log_files[0])
    print(f"Analyzing: {latest_log}")
    
    results = parse_hw_test_log(latest_log)
    print_summary(results)
    
    # Exit with error code if any tests failed
    sys.exit(1 if results['fail'] else 0)

if __name__ == '__main__':
    main()
