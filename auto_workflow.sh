#!/bin/bash

# Wyn v1.6.0 - Automated Workflow with Kiro CLI
# This script reads the roadmap and invokes Kiro CLI to implement each task

set -e

ROADMAP_JSON="v1.6.0_roadmap.json"
ROADMAP_MD="V1.6.0_COMPLETE_ROADMAP.md"
LOG_FILE="v1.6.0_workflow.log"
KIRO_PROMPT_FILE=".kiro_task_prompt.txt"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}✗${NC} $1" | tee -a "$LOG_FILE"
}

log_info() {
    echo -e "${BLUE}ℹ${NC} $1" | tee -a "$LOG_FILE"
}

# Get next task from roadmap
get_next_task() {
    # Parse JSON to find first task with status "not_started"
    if command -v jq &> /dev/null; then
        jq -r '.epics[].tasks[] | select(.status == "not_started") | .id' "$ROADMAP_JSON" | head -1
    else
        # Fallback: parse markdown
        grep -A 1 "🔴 Not Started" "$ROADMAP_MD" | grep "Task" | head -1 | sed 's/.*Task //' | sed 's/:.*//'
    fi
}

# Get task details
get_task_details() {
    local task_id=$1
    
    if command -v jq &> /dev/null; then
        jq -r ".epics[].tasks[] | select(.id == \"$task_id\")" "$ROADMAP_JSON"
    else
        log_error "jq not found. Install jq for JSON parsing."
        return 1
    fi
}

# Create Kiro prompt for task
create_kiro_prompt() {
    local task_id=$1
    local task_name=$2
    local definition_of_done=$3
    local tests=$4
    local files=$5
    local pass=$6
    
    if [ "$pass" == "1" ]; then
        # First pass: Initial implementation
        cat > "$KIRO_PROMPT_FILE" << EOF
TASK: $task_id - $task_name (PASS 1: Initial Implementation)

DEFINITION OF DONE:
$definition_of_done

REQUIRED TESTS:
$tests

FILES TO MODIFY:
$files

INSTRUCTIONS FOR PASS 1:
1. Read the task requirements above
2. Analyze existing codebase to understand current state
3. Implement the feature using TDD:
   - Create test files first
   - Implement minimal working solution
   - Make tests pass
4. Run full regression to ensure no breakage
5. Document what you implemented

GOAL: Get it working correctly, don't worry about optimization yet.

Start implementing now.
EOF
    elif [ "$pass" == "2" ]; then
        # Second pass: Review and improve
        cat > "$KIRO_PROMPT_FILE" << EOF
TASK: $task_id - $task_name (PASS 2: Review & Improve)

PREVIOUS IMPLEMENTATION: You completed the initial implementation in Pass 1.

INSTRUCTIONS FOR PASS 2:
1. Review your Pass 1 implementation critically
2. Identify issues:
   - Code quality problems
   - Performance issues
   - Missing edge cases
   - Better design patterns
   - Cleaner abstractions
3. Refactor and improve the implementation
4. Add more comprehensive tests
5. Ensure all tests still pass

GOAL: Make it better - cleaner, faster, more robust.

Start reviewing and improving now.
EOF
    elif [ "$pass" == "3" ]; then
        # Third pass: Optimize and polish
        cat > "$KIRO_PROMPT_FILE" << EOF
TASK: $task_id - $task_name (PASS 3: Optimize & Polish)

PREVIOUS PASSES: You've implemented and improved the feature.

INSTRUCTIONS FOR PASS 3:
1. Review the current implementation one more time
2. Optimize for:
   - Performance (speed and memory)
   - Code clarity and maintainability
   - Error handling
   - Edge cases
3. Add documentation and examples
4. Final validation:
   - All tests pass
   - No regressions
   - Code is production-ready

GOAL: Make it optimal - this is the final version.

Start optimizing and polishing now.
EOF
    fi
    
    log_info "Created Kiro prompt for Pass $pass: $KIRO_PROMPT_FILE"
}

# Execute task with Kiro CLI (3 passes)
execute_task_with_kiro() {
    local task_id=$1
    
    log "=== Executing Task: $task_id with Kiro CLI (3-Pass System) ==="
    
    # Get task details from JSON
    local task_json=$(get_task_details "$task_id")
    
    if [ -z "$task_json" ]; then
        log_error "Task $task_id not found in roadmap"
        return 1
    fi
    
    # Extract task info
    local task_name=$(echo "$task_json" | jq -r '.name')
    local definition_of_done=$(echo "$task_json" | jq -r '.definition_of_done | join("\n")')
    local tests=$(echo "$task_json" | jq -r '.tests | join("\n")')
    local files=$(echo "$task_json" | jq -r '.files_to_modify | join("\n")')
    
    log_info "Task: $task_name"
    
    # PASS 1: Initial Implementation
    log "--- PASS 1: Initial Implementation ---"
    create_kiro_prompt "$task_id" "$task_name" "$definition_of_done" "$tests" "$files" "1"
    
    if command -v kiro-cli &> /dev/null; then
        cat "$KIRO_PROMPT_FILE" | kiro-cli chat
    else
        log_error "kiro-cli not found"
        return 1
    fi
    
    if ! validate_task "$task_id"; then
        log_error "Pass 1 validation failed"
        return 1
    fi
    log_success "Pass 1 complete"
    
    # PASS 2: Review & Improve
    log "--- PASS 2: Review & Improve ---"
    create_kiro_prompt "$task_id" "$task_name" "$definition_of_done" "$tests" "$files" "2"
    
    cat "$KIRO_PROMPT_FILE" | kiro-cli chat
    
    if ! validate_task "$task_id"; then
        log_error "Pass 2 validation failed"
        return 1
    fi
    log_success "Pass 2 complete"
    
    # PASS 3: Optimize & Polish
    log "--- PASS 3: Optimize & Polish ---"
    create_kiro_prompt "$task_id" "$task_name" "$definition_of_done" "$tests" "$files" "3"
    
    cat "$KIRO_PROMPT_FILE" | kiro-cli chat
    
    if ! validate_task "$task_id"; then
        log_error "Pass 3 validation failed"
        return 1
    fi
    log_success "Pass 3 complete"
    
    # Final validation
    log_info "Running final validation..."
    if validate_task "$task_id"; then
        log_success "Task $task_id completed successfully with 3 passes!"
        update_roadmap "$task_id" "complete"
        return 0
    else
        log_error "Final validation failed"
        return 1
    fi
}

# Validate task completion
validate_task() {
    local task_id=$1
    
    log_info "Running validation for $task_id..."
    
    # Get test files for this task
    local tests=$(jq -r ".epics[].tasks[] | select(.id == \"$task_id\") | .tests[]" "$ROADMAP_JSON")
    
    # Run each test
    for test_file in $tests; do
        if [ -f "$test_file" ]; then
            log_info "Running test: $test_file"
            
            # Compile test
            if ! ./wyn "$test_file" > /dev/null 2>&1; then
                log_error "Test compilation failed: $test_file"
                return 1
            fi
            
            # Run test
            local output_file="${test_file%.wyn}.out"
            if [ -f "$output_file" ]; then
                if ! timeout 5s "$output_file" > /dev/null 2>&1; then
                    log_error "Test execution failed: $test_file"
                    return 1
                fi
            fi
            
            log_success "Test passed: $test_file"
        else
            log_warning "Test file not found: $test_file (will be created by Kiro)"
        fi
    done
    
    # Run regression
    log_info "Running regression tests..."
    if ! run_regression; then
        log_error "Regression failed"
        return 1
    fi
    
    log_success "All validations passed"
    return 0
}

# Run regression tests
run_regression() {
    # Run existing test suite
    local failed=0
    
    # Count passing tests
    for test_file in tests/**/*.wyn; do
        if [ -f "$test_file" ]; then
            if ! ./wyn "$test_file" > /dev/null 2>&1; then
                failed=$((failed + 1))
            fi
        fi
    done
    
    if [ $failed -gt 0 ]; then
        log_error "Regression: $failed tests failed"
        return 1
    fi
    
    log_success "Regression: All tests passed"
    return 0
}

# Update roadmap status
update_roadmap() {
    local task_id=$1
    local status=$2
    
    log_info "Updating roadmap: $task_id -> $status"
    
    # Update JSON
    if command -v jq &> /dev/null; then
        local tmp_file=$(mktemp)
        jq "(.epics[].tasks[] | select(.id == \"$task_id\") | .status) = \"$status\"" "$ROADMAP_JSON" > "$tmp_file"
        mv "$tmp_file" "$ROADMAP_JSON"
        
        # Update progress counters
        local complete=$(jq '[.epics[].tasks[] | select(.status == "complete")] | length' "$ROADMAP_JSON")
        local total=$(jq '[.epics[].tasks[]] | length' "$ROADMAP_JSON")
        jq ".progress.tasks_complete = $complete" "$ROADMAP_JSON" > "$tmp_file"
        mv "$tmp_file" "$ROADMAP_JSON"
        
        log_success "Roadmap updated: $complete/$total tasks complete"
    fi
    
    # Update Markdown (replace 🔴 with 🟢 for this task)
    sed -i.bak "s/\*\*Status\*\*: 🔴 Not Started.*$task_id/**Status**: 🟢 Complete - $task_id/" "$ROADMAP_MD"
    
    # Commit changes
    git add "$ROADMAP_JSON" "$ROADMAP_MD"
    git commit -m "progress: Complete $task_id" || true
}

# Main execution
main() {
    log "=== Wyn v1.6.0 Automated Workflow with Kiro CLI ==="
    log "This script will invoke Kiro CLI to implement each task systematically"
    echo ""
    
    # Check prerequisites
    if ! command -v jq &> /dev/null; then
        log_error "jq is required but not installed. Install with: brew install jq"
        exit 1
    fi
    
    if ! command -v kiro-cli &> /dev/null; then
        log_warning "kiro-cli not found in PATH"
        log_info "Will generate prompts that you can run manually"
    fi
    
    # Get next task
    local task_id=$(get_next_task)
    
    if [ -z "$task_id" ]; then
        log_success "All tasks complete! 🎉"
        exit 0
    fi
    
    log_info "Next task: $task_id"
    
    # Execute task
    if execute_task_with_kiro "$task_id"; then
        log_success "Task completed successfully!"
        
        # Continue to next task
        log_info "Moving to next task..."
        exec "$0"  # Re-run script for next task
    else
        log_error "Task failed. Fix issues and run again."
        exit 1
    fi
}

# Run main
main "$@"
