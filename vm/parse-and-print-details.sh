#!/usr/bin/env bash
#
# console_output.sh
# 사용법:
#   1) 콘솔 출력을 파일로 저장했다면:
#        ./console_output.sh output.txt
#   2) 파이프라인으로 바로 넘겨주고 싶다면:
#        cat output.txt | ./console_output.sh
#
# 스크립트 동작:
#   - 각 줄을 읽으면서 “FAIL tests/…” 패턴에 매치되는지 확인
#   - 매치되면 tests/ 뒤의 경로에서 마지막 “/테스트명”을 제거하여
#     디렉터리(예: userprog, vm, filesys/base 등)를 추출
#   - 해당 디렉터리를 키로 한 associative 배열에 ++ 하여 개수 집계
#   - 마지막에 “디렉터리: FAIL 개수” 형태로 출력

# bash 4 이상의 associative array를 사용합니다.
if [[ "${BASH_VERSINFO[0]}" -lt 4 ]]; then
  echo "ERROR: 이 스크립트는 Bash 4.0 이상에서만 동작합니다." >&2
  exit 1
fi

# 입력 파일을 인수로 받거나, 인자가 없으면 stdin 사용
if [[ $# -gt 1 ]]; then
  echo "Usage: $0 [console_output.txt]" >&2
  exit 1
fi

if [[ $# -eq 1 ]]; then
  INPUT="$1"
else
  INPUT="/dev/stdin"
fi

declare -A fail_counts

# 한 줄씩 읽으면서 FAIL 패턴을 찾고, 디렉터리 부분만 추출하여 카운트
while IFS= read -r line; do
  # “FAIL tests/...” 형태에만 매치
  if [[ $line == FAIL\ tests/* ]]; then
    # “FAIL tests/” 이후 문자열만 남김
    path_part=${line#FAIL tests/}
    # 마지막 “/테스트명”을 떼어내서 디렉터리 경로만 남김
    dir=${path_part%/*}
    # associative array에 ++
    (( fail_counts["$dir"]++ ))
  fi
done < "$INPUT"

# 결과 출력
echo "================ FAIL 개수 집계 ================="
if [[ ${#fail_counts[@]} -eq 0 ]]; then
  echo "FAIL 항목이 없습니다."
  exit 0
fi

# 키(디렉터리) 순서대로 정렬해서 출력
for dir in $(printf "%s\n" "${!fail_counts[@]}" | sort); do
  printf "%-20s : %d\n" "$dir" "${fail_counts[$dir]}"
done
echo "==============================================="

