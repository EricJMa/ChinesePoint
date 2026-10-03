"""Execute the workflow's two-worktree shell plumbing with bounded fake builds."""
import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[2]


def workflow_script():
    lines = (ROOT / '.github/workflows/chinesepoint-simulator.yml').read_text().splitlines()
    start = lines.index('      - name: Build the same X4 Pro commit in two worktrees') + 2
    block = []
    for line in lines[start:]:
        if line and not line.startswith('          '):
            break
        block.append(line[10:])
    return '\n'.join(block)


class DeterministicWorkflowTest(unittest.TestCase):
    def run_workflow(self, script, mismatch=False):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            workspace = root / 'source'
            bins = root / 'bin'
            workspace.mkdir()
            bins.mkdir()
            commands = {
                'git': '#!/bin/bash\nif [ "$1" = worktree ]; then mkdir -p "$5"; fi\n',
                'pio': '''#!/bin/bash
mkdir -p .pio/build/chinesepoint_x4pro
for artifact in firmware.bin update.bin; do
  printf 'repeatable firmware fixture' > ".pio/build/chinesepoint_x4pro/$artifact"
  if [ "${TEST_MISMATCH:-0}" = 1 ] && [ "${PWD##*/}" = chinesepoint-x4pro-rebuild ]; then
    printf 'changed' >> ".pio/build/chinesepoint_x4pro/$artifact"
  fi
done
echo 'fake build completed'
''',
            }
            # macOS names this utility shasum; preserve the workflow's interface.
            if not any((Path(part) / 'sha256sum').exists() for part in os.environ['PATH'].split(os.pathsep)):
                commands['sha256sum'] = '#!/bin/bash\nexec shasum -a 256 "$@"\n'
            for name, code in commands.items():
                path = bins / name
                path.write_text(textwrap.dedent(code))
                path.chmod(0o755)
            env = dict(os.environ, PATH=str(bins) + os.pathsep + os.environ['PATH'],
                       GITHUB_WORKSPACE=str(workspace), GITHUB_SHA='test-commit', TEST_MISMATCH=str(int(mismatch)))
            result = subprocess.run(['bash', '-c', script], cwd=workspace, env=env, capture_output=True, text=True)
            logs = [(workspace / 'qa-artifacts' / name).exists() for name in ['pio-first.log', 'pio-second.log']]
            hashes = (workspace / 'qa-artifacts/firmware.bin.sha256').exists()
            return result, logs, hashes

    def test_fixed_workflow_keeps_both_logs_and_compares_artifacts(self):
        result, logs, hashes = self.run_workflow(workflow_script())
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(logs, [True, True])
        self.assertTrue(hashes)

    def test_original_relative_second_log_fails_before_comparison(self):
        original = workflow_script().replace('"$GITHUB_WORKSPACE/qa-artifacts/pio-second.log"', 'qa-artifacts/pio-second.log')
        result, _, hashes = self.run_workflow(original)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('pio-second.log', result.stderr)
        self.assertFalse(hashes)

    def test_different_second_build_is_still_rejected(self):
        result, _, hashes = self.run_workflow(workflow_script(), mismatch=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Non-deterministic', result.stdout)
        self.assertTrue(hashes)


if __name__ == '__main__':
    unittest.main()
