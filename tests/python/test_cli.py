from __future__ import annotations

import subprocess
import unittest
from pathlib import Path
from unittest import mock

from cuda_doctor import cli


class CliTests(unittest.TestCase):
    def test_main_forwards_check_to_native_binary(self) -> None:
        completed = subprocess.CompletedProcess(
            args=["cuda-doctor-core", "check"],
            returncode=0,
        )

        with (
            mock.patch("cuda_doctor.cli.resolve_core_binary", return_value=Path("/tmp/cuda-doctor-core")),
            mock.patch("cuda_doctor.cli.subprocess.run", return_value=completed) as run_mock,
        ):
            exit_code = cli.main(["check"])

        self.assertEqual(0, exit_code)
        run_mock.assert_called_once_with(
            ["/tmp/cuda-doctor-core", "check"],
            check=False,
        )

    def test_main_forwards_doctor_json_to_native_binary(self) -> None:
        completed = subprocess.CompletedProcess(
            args=["cuda-doctor-core", "doctor", "--json"],
            returncode=1,
        )

        with (
            mock.patch("cuda_doctor.cli.resolve_core_binary", return_value=Path("/tmp/cuda-doctor-core")),
            mock.patch("cuda_doctor.cli.subprocess.run", return_value=completed) as run_mock,
        ):
            exit_code = cli.main(["doctor", "--json"])

        self.assertEqual(1, exit_code)
        run_mock.assert_called_once_with(
            ["/tmp/cuda-doctor-core", "doctor", "--json"],
            check=False,
        )
