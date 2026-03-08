# cuda-doctor

`cuda-doctor` is a diagnose + repair + build + validate CLI for CUDA environments.

It is not a replacement for `nvidia-smi`, `cuda-gdb`, or `compute-sanitizer`.
Those tools expose low level state or debugging workflows. `cuda-doctor` sits one
level above them and helps fix one problem I personally had when working on cuda:

> Can this machine build and run real CUDA workloads correctly, and if not, can
> it fix the environment safely?

## Project

`cuda-doctor` should help developers recover from broken or misleading CUDA
installs, especially on newer NVIDIA hardware where the stack often fails in
non obvious ways.

Areas:

- RTX 5000-series / Blackwell readiness
- Missing `sm_120` support in toolchains or build flags
- Outdated CUDA, PyTorch, or related runtime stacks
- Wrong Linux kernel module flavor
- Driver/runtime mismatches
- "Fake success" installs where CUDA appears present but real GPU execution fails

The project should act like an environment doctor on the CLI

## User use

The main user journey should be simple:

1. Install `cuda-doctor`
2. Run `cuda-doctor doctor`
3. See a clear diagnosis of what is broken, what is risky, and what can be fixed
4. Run `cuda-doctor doctor auto` to apply compatible repairs
5. Run `cuda-doctor validate` to prove real GPU execution works
6. Run `cuda-doctor build` inside a project to compile CUDA code with the right
   architecture and toolchain settings

The user should not need to manually reason about:

- driver branch compatibility
- toolkit/runtime drift
- architecture flags for new GPUs
- kernel module flavor mismatches
- whether a reported CUDA install is actually usable

## Command Model

### `cuda-doctor doctor`

Runs a full environment diagnosis and reports:

- GPU model, compute capability, VRAM, and architecture family
- installed driver version and driver health
- CUDA toolkit version, `nvcc` availability, and architecture support
- runtime/library versions relevant to real workloads
- PyTorch compatibility and whether the installed wheel can target the local GPU
- build chain readiness for CUDA projects
- validation risk level if the environment looks superficially correct but likely
  fails under actual execution

This command should explicitly detect failure modes like:

- GPU is Blackwell class but the local toolchain cannot target `sm_120`
- driver is new enough for `nvidia-smi` to work but too old for the intended runtime
- PyTorch is installed but built against an incompatible CUDA runtime
- Linux is using the wrong NVIDIA kernel module flavor for the desired stack
- `nvcc` exists, but an actual kernel launch or memory transfer fails

### `cuda-doctor doctor auto`

This is the repair mode.

It should act like `npm audit fix` conceptually:

- inspect the current environment
- identify broken, outdated, or incompatible CUDA-related components
- upgrade or reconfigure them automatically
- prefer the newest compatible versions rather than blindly forcing latest-everything

The compatibility rule is critical:

- preserve the user's existing environment constraints where possible
- move to secure/working/compatible versions within the current project or system
  envelope
- only make larger compatibility jumps when the existing stack cannot support the
  detected GPU or requested workload

Examples of what `doctor auto` should repair:

- update build flags to include the correct `sm_*` architecture target
- replace an outdated CUDA toolkit that cannot support the detected GPU
- surface and fix a mismatched PyTorch wheel/runtime pairing
- correct path or environment issues so the intended toolkit is actually used
- repair common Linux driver/module mismatches
- refuse a fake-success state until a real validation kernel passes

`doctor auto` WILL not claim success based only on package presence. A repair is
complete only if the environment passes validation.

### `cuda-doctor check`

Runs a read only, lighter weight inspection for CI, scripting, or quick triage.

### `cuda-doctor setup`

Prepares a machine for CUDA use from a mostly missing or incomplete state. This is
the install/bootstrap path, while `doctor auto` is the repair/reconcile path.

### `cuda-doctor build`

Builds CUDA code in the current project using the correct toolkit, compiler, and
architecture settings for the local machine.

This should shield users from hand authoring the right flags for new hardware.

### `cuda-doctor validate`

Runs a real smoke test against the GPU

Validation should confirm:

- device selection works
- memory allocation and transfer work
- a simple kernel launches and completes
- results are numerically sane
- the runtime/toolkit/driver combination works in practice

If `validate` fails, the install is not healthy, even if other tools suggest it is.

## Did we succeed?

It should be good if

- an installable `cuda-doctor` CLI
- a native core that inspects the system and interacts with CUDA directly
- a repair engine that can reconcile broken or outdated environments
- a build helper for real CUDA projects
- a validation path that proves actual GPU execution
- machine readable reports for automation and CI

## Repository

- `src/`: native command routing, diagnosis, repair, build, validation, and
  terminal/report output
- `include/`: headers mirroring native modules
- `kernels/`: CUDA smoke tests and benchmark kernels used for validation
- `cuda_doctor/`: Python CLI wrapper, config handling, and rich output
- `tests/`: C++ unit tests and Python CLI tests
- `docker/`: reproducible CUDA build and validation environments
- `scripts/`: bootstrap/setup automation
- `CMakeLists.txt`: build graph for the native C++/CUDA core
- `pyproject.toml`: Python package and CLI entry point definition

## Success

`cuda-doctor` is successful when a user can run one command and fix CUDA issues

- is my CUDA environment actually usable?
- if not, what exactly is wrong?
- can the tool repair it automatically?
- can this machine build and run CUDA workloads for this GPU generation?
