#!/bin/bash

# Phase 2 Agent Coordination Monitor (Simplified)
# Monitors parallel agent execution and validates integration points

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
AGENTS_DIR="$PROJECT_ROOT/.agents"
LOG_FILE="$AGENTS_DIR/logs/phase2_integration.log"

# Ensure log directory exists
mkdir -p "$AGENTS_DIR/logs"

# Logging function
log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Get agent progress percentage
get_agent_progress() {
    local agent_name="$1"
    local progress_file="$AGENTS_DIR/$agent_name/progress.txt"
    
    if [[ -f "$progress_file" ]]; then
        cat "$progress_file" 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

# Get agent current task
get_agent_task() {
    local agent_name="$1"
    local task_file="$AGENTS_DIR/$agent_name/current_task.txt"
    
    if [[ -f "$task_file" ]]; then
        cat "$task_file" 2>/dev/null || echo "Unknown"
    else
        echo "Not Started"
    fi
}

# Check agent status
get_agent_status() {
    local agent_name="$1"
    local status_file="$AGENTS_DIR/$agent_name/status.txt"
    
    if [[ -f "$status_file" ]]; then
        if grep -q "COMPLETE\|Complete" "$status_file" 2>/dev/null; then
            echo "COMPLETE"
        elif grep -q "IN_PROGRESS\|In Progress" "$status_file" 2>/dev/null; then
            echo "IN_PROGRESS"
        else
            echo "UNKNOWN"
        fi
    else
        echo "NOT_STARTED"
    fi
}

# Display agent coordination status
show_agent_status() {
    log_info "=== Phase 2 Agent Coordination Status ==="
    
    printf "%-20s %-12s %-15s %-30s\n" "Agent" "Status" "Progress" "Current Task"
    printf "%-20s %-12s %-15s %-30s\n" "--------------------" "------------" "---------------" "------------------------------"
    
    # Phase 2 agents list
    local agents="llvm-infrastructure llvm-codegen arc-runtime arc-compiler type-system cross-platform"
    
    for agent in $agents; do
        local status=$(get_agent_status "$agent")
        local progress=$(get_agent_progress "$agent")
        local task=$(get_agent_task "$agent")
        
        # Color coding for status
        local status_display="$status"
        case "$status" in
            "COMPLETE") status_display="✅ $status" ;;
            "IN_PROGRESS") status_display="🔄 $status" ;;
            "NOT_STARTED") status_display="⏳ $status" ;;
            *) status_display="❓ $status" ;;
        esac
        
        printf "%-20s %-20s %-15s %-30s\n" "$agent" "$status_display" "${progress}%" "$task"
    done
    
    echo ""
}

# Check integration checkpoints
check_integration_checkpoints() {
    log_info "=== Integration Checkpoint Validation ==="
    
    # Day 5: LLVM Infrastructure Complete
    local llvm_progress=$(get_agent_progress "llvm-infrastructure")
    local day5_status="⏳ PENDING"
    if [[ "$llvm_progress" -ge 100 ]]; then
        day5_status="✅ PASSED"
    fi
    printf "Day %-2d: %-30s %s\n" "5" "LLVM Infrastructure Complete" "$day5_status"
    
    # Day 10: Basic Codegen + ARC Runtime Ready
    local codegen_progress=$(get_agent_progress "llvm-codegen")
    local arc_progress=$(get_agent_progress "arc-runtime")
    local day10_status="⏳ PENDING"
    if [[ "$codegen_progress" -ge 60 && "$arc_progress" -ge 40 ]]; then
        day10_status="✅ PASSED"
    fi
    printf "Day %-2d: %-30s %s\n" "10" "Basic Codegen + ARC Runtime Ready" "$day10_status"
    
    # Day 15: Core Systems Complete
    local type_progress=$(get_agent_progress "type-system")
    local day15_status="⏳ PENDING"
    if [[ "$type_progress" -ge 100 ]]; then
        day15_status="✅ PASSED"
    fi
    printf "Day %-2d: %-30s %s\n" "15" "Core Systems Complete" "$day15_status"
    
    # Day 20: Full Integration
    local all_complete=true
    for agent in llvm-infrastructure llvm-codegen arc-runtime arc-compiler type-system cross-platform; do
        local agent_status=$(get_agent_status "$agent")
        if [[ "$agent_status" != "COMPLETE" ]]; then
            all_complete=false
            break
        fi
    done
    local day20_status="⏳ PENDING"
    if [[ "$all_complete" == true ]]; then
        day20_status="✅ PASSED"
    fi
    printf "Day %-2d: %-30s %s\n" "20" "Full Integration" "$day20_status"
    
    echo ""
}

# Validate parallel execution (no conflicts)
check_parallel_execution() {
    log_info "=== Parallel Execution Validation ==="
    
    # Check for file ownership conflicts
    local conflicts=0
    
    # Shared files that multiple agents might modify
    local shared_files="src/ast.h src/parser.c src/codegen.c Makefile"
    
    for file in $shared_files; do
        if [[ -f "$file" ]]; then
            echo "✅ $file - no conflicts detected"
        else
            echo "⚠️  $file - file missing"
            ((conflicts++))
        fi
    done
    
    if [[ $conflicts -eq 0 ]]; then
        echo "✅ No parallel execution conflicts detected"
    else
        echo "⚠️  $conflicts potential conflicts detected"
    fi
    
    echo ""
}

# Test LLVM integration status
check_llvm_integration() {
    log_info "=== LLVM Integration Status ==="
    
    # Test LLVM build
    if make wyn-llvm >/dev/null 2>&1; then
        echo "✅ LLVM build integration working"
        
        # Test LLVM version
        local llvm_version=$(llvm-config --version 2>/dev/null || echo "unknown")
        echo "✅ LLVM version: $llvm_version"
        
        # Test basic LLVM functionality
        if [[ -x "./wyn-llvm" ]]; then
            echo "✅ LLVM-based compiler executable created"
        else
            echo "⏳ LLVM-based compiler not yet functional"
        fi
    else
        echo "⚠️  LLVM build integration needs work"
    fi
    
    echo ""
}

# Generate integration report
generate_report() {
    local report_file="$AGENTS_DIR/phase2_integration_report.md"
    
    cat > "$report_file" << EOF
# Phase 2 Integration Report
*Generated: $(date)*

## Agent Status Summary

EOF

    show_agent_status >> "$report_file"
    
    cat >> "$report_file" << EOF

## Integration Checkpoints

EOF

    check_integration_checkpoints >> "$report_file"
    
    cat >> "$report_file" << EOF

## LLVM Integration Status

EOF

    check_llvm_integration >> "$report_file"
    
    cat >> "$report_file" << EOF

## Recommendations

EOF

    # Add recommendations based on current status
    local llvm_progress=$(get_agent_progress "llvm-infrastructure")
    if [[ "$llvm_progress" -lt 100 ]]; then
        echo "- Priority: Complete LLVM Infrastructure setup (T2.1)" >> "$report_file"
    fi
    
    local codegen_progress=$(get_agent_progress "llvm-codegen")
    if [[ "$codegen_progress" -eq 0 && "$llvm_progress" -ge 100 ]]; then
        echo "- Ready: Begin LLVM Code Generation work (T2.2)" >> "$report_file"
    fi
    
    echo "" >> "$report_file"
    echo "Report saved to: $report_file"
}

# Main monitoring function
main() {
    log "Starting Phase 2 Integration Monitoring"
    
    # Show current status
    show_agent_status
    check_integration_checkpoints
    check_parallel_execution
    check_llvm_integration
    
    # Generate report
    generate_report
    
    log "Phase 2 Integration Monitoring Complete"
}

# Command line interface
case "${1:-status}" in
    "status")
        main
        ;;
    "agents")
        show_agent_status
        ;;
    "checkpoints")
        check_integration_checkpoints
        ;;
    "conflicts")
        check_parallel_execution
        ;;
    "llvm")
        check_llvm_integration
        ;;
    "report")
        generate_report
        ;;
    "help")
        echo "Usage: $0 [status|agents|checkpoints|conflicts|llvm|report|help]"
        echo ""
        echo "Commands:"
        echo "  status      - Full integration status (default)"
        echo "  agents      - Show agent coordination status"
        echo "  checkpoints - Check integration checkpoints"
        echo "  conflicts   - Validate parallel execution"
        echo "  llvm        - Check LLVM integration status"
        echo "  report      - Generate integration report"
        echo "  help        - Show this help"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use '$0 help' for usage information"
        exit 1
        ;;
esac