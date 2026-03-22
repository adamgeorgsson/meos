#!/bin/bash
# Ralph Wiggum - Long-running AI agent loop
# Usage: ./ralph.sh [--tool claude|copilot] [max_iterations]
#
# ITERATIVE MIGRATION: This script runs the entire migration from scratch.
# After each full run, we analyze results (progress.txt, metrics.csv),
# improve the PRD/skills/prompt.md, and run again. The generated code is
# disposable — only the learnings persist across iterations.
#
# Before running: sync with upstream (code/ is a fork of melinsoftware/meos)
# and ensure plan/prd.json is regenerated from the PRD.

set -e

# Clean exit on interrupt — don't write garbage to progress/metrics
trap 'echo ""; echo "Ralph interrupted. No partial data written."; exit 130' INT TERM

# Parse arguments
TOOL="claude"
MAX_ITERATIONS=10
MODEL="claude-sonnet-4.6"

while [[ $# -gt 0 ]]; do
  case $1 in
    --tool)
      TOOL="$2"
      shift 2
      ;;
    --tool=*)
      TOOL="${1#*=}"
      shift
      ;;
    --model)
      MODEL="$2"
      shift 2
      ;;
    --model=*)
      MODEL="${1#*=}"
      shift
      ;;
    *)
      # Assume it's max_iterations if it's a number
      if [[ "$1" =~ ^[0-9]+$ ]]; then
        MAX_ITERATIONS="$1"
      fi
      shift
      ;;
  esac
done

# Validate tool choice
if [[ "$TOOL" != "claude" && "$TOOL" != "copilot" ]]; then
  echo "Error: Invalid tool '$TOOL'. Must be 'claude' or 'copilot'."
  exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRD_FILE="$SCRIPT_DIR/prd.json"
PROGRESS_FILE="$SCRIPT_DIR/progress.txt"
METRICS_FILE="$SCRIPT_DIR/metrics.csv"

# Initialize progress file if it doesn't exist
if [ ! -f "$PROGRESS_FILE" ]; then
  echo "# Ralph Progress Log" > "$PROGRESS_FILE"
  echo "Started: $(date)" >> "$PROGRESS_FILE"
  echo "---" >> "$PROGRESS_FILE"
fi

# Initialize metrics CSV if it doesn't exist
if [ ! -f "$METRICS_FILE" ]; then
  echo "task_id,tool,start_time,duration_seconds,status,cost_usd,input_tokens,output_tokens" > "$METRICS_FILE"
fi

echo "Starting Ralph - Tool: $TOOL - Model: $MODEL - Max iterations: $MAX_ITERATIONS"

for i in $(seq 1 $MAX_ITERATIONS); do
  echo ""
  echo "==============================================================="
  echo "  Ralph Iteration $i of $MAX_ITERATIONS ($TOOL)"
  echo "==============================================================="

  STEP_START=$(date +%s)
  STEP_START_FMT=$(date -Iseconds)

  # Snapshot stories that haven't passed yet (to detect which one gets completed)
  BEFORE_FAILING=$(jq -r '.userStories[] | select(.passes != true) | .id' "$PRD_FILE" 2>/dev/null | sort)

  # Run the selected tool with the ralph prompt (60 min timeout per iteration)
  # --kill-after=10: send SIGKILL 10s after SIGTERM if process won't die
  TIMEOUT=3600
  COST_USD=""
  INPUT_TOKENS=""
  OUTPUT_TOKENS=""
  if [[ "$TOOL" == "claude" ]]; then
    CLAUDE_MODEL="${MODEL//./-}"
    JSON_OUTPUT=$(timeout --kill-after=10 $TIMEOUT claude --dangerously-skip-permissions --print --output-format json --model "$CLAUDE_MODEL" < "$SCRIPT_DIR/prompt.md" 2>/dev/null) || true
    # Extract text result and usage from JSON
    OUTPUT=$(echo "$JSON_OUTPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('result',''))" 2>/dev/null) || OUTPUT="$JSON_OUTPUT"
    COST_USD=$(echo "$JSON_OUTPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); print(f'{d.get(\"total_cost_usd\",0):.4f}')" 2>/dev/null) || true
    INPUT_TOKENS=$(echo "$JSON_OUTPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); u=d.get('usage',{}); print(u.get('input_tokens',0) + u.get('cache_read_input_tokens',0) + u.get('cache_creation_input_tokens',0))" 2>/dev/null) || true
    OUTPUT_TOKENS=$(echo "$JSON_OUTPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('usage',{}).get('output_tokens',0))" 2>/dev/null) || true
    echo "$OUTPUT" >&2
  elif [[ "$TOOL" == "copilot" ]]; then
    OUTPUT=$(timeout --kill-after=10 $TIMEOUT copilot -p "$(cat "$SCRIPT_DIR/prompt.md")" --allow-all --model "$MODEL" 2>&1 | tee /dev/stderr) || true
  fi

  STEP_END=$(date +%s)
  STEP_DURATION=$((STEP_END - STEP_START))
  STEP_DURATION_FMT=$(printf '%dm%02ds' $((STEP_DURATION / 60)) $((STEP_DURATION % 60)))

  # Detect which task was completed by comparing before/after
  AFTER_FAILING=$(jq -r '.userStories[] | select(.passes != true) | .id' "$PRD_FILE" 2>/dev/null | sort)
  TASK_ID=$(comm -23 <(echo "$BEFORE_FAILING") <(echo "$AFTER_FAILING") | head -1)
  if [ -z "$TASK_ID" ]; then
    TASK_ID="iteration-$i"
  fi

  # Check for completion signal
  if echo "$OUTPUT" | grep -q "<promise>COMPLETE</promise>"; then
    echo ""
    echo "Ralph completed all tasks!"
    echo "Completed at iteration $i of $MAX_ITERATIONS (${STEP_DURATION_FMT})"

    # Log final metrics with token usage
    echo "$TASK_ID,$TOOL,$STEP_START_FMT,$STEP_DURATION,completed,$COST_USD,$INPUT_TOKENS,$OUTPUT_TOKENS" >> "$METRICS_FILE"
    echo "" >> "$PROGRESS_FILE"
    echo "## $TASK_ID — COMPLETED (${STEP_DURATION_FMT})" >> "$PROGRESS_FILE"

    # Print summary
    echo ""
    echo "=== Metrics Summary (see $METRICS_FILE) ==="
    TOTAL_TIME=0
    TOTAL_COST=0
    TASK_COUNT=0
    while IFS=',' read -r tid tool start dur status cost in_tok out_tok; do
      [[ "$tid" == "task_id" ]] && continue
      TOTAL_TIME=$((TOTAL_TIME + dur))
      TASK_COUNT=$((TASK_COUNT + 1))
      if [[ -n "$cost" ]]; then
        TOTAL_COST=$(python3 -c "print(round($TOTAL_COST + $cost, 4))")
      fi
    done < "$METRICS_FILE"
    printf "Total time: %dm%02ds across %d tasks, total cost: \$%s\n" $((TOTAL_TIME / 60)) $((TOTAL_TIME % 60)) "$TASK_COUNT" "$TOTAL_COST"
    exit 0
  fi

  # Log metrics for this iteration with token usage
  echo "$TASK_ID,$TOOL,$STEP_START_FMT,$STEP_DURATION,continuing,$COST_USD,$INPUT_TOKENS,$OUTPUT_TOKENS" >> "$METRICS_FILE"
  echo "" >> "$PROGRESS_FILE"
  echo "## $TASK_ID (${STEP_DURATION_FMT})" >> "$PROGRESS_FILE"

  echo "Iteration $i complete (${STEP_DURATION_FMT}). Continuing..."
  sleep 2
done

echo ""
echo "Ralph reached max iterations ($MAX_ITERATIONS) without completing all tasks."
echo "Check $PROGRESS_FILE for status."
echo ""
echo "=== Metrics Summary (see $METRICS_FILE) ==="
TOTAL_TIME=0
TOTAL_COST=0
TASK_COUNT=0
while IFS=',' read -r tid tool start dur status cost in_tok out_tok; do
  [[ "$tid" == "task_id" ]] && continue
  TOTAL_TIME=$((TOTAL_TIME + dur))
  TASK_COUNT=$((TASK_COUNT + 1))
  if [[ -n "$cost" ]]; then
    TOTAL_COST=$(python3 -c "print(round($TOTAL_COST + $cost, 4))")
  fi
  printf "  %s: %dm%02ds \$%s\n" "$tid" $((dur / 60)) $((dur % 60)) "${cost:-n/a}"
done < "$METRICS_FILE"
printf "Total time: %dm%02ds across %d tasks, total cost: \$%s\n" $((TOTAL_TIME / 60)) $((TOTAL_TIME % 60)) "$TASK_COUNT" "$TOTAL_COST"
exit 1
