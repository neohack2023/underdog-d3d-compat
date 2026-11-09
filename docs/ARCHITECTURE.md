# Architecture and acceptance

Native hardware first, GPU compute fallback second, explicit CPU reference last. Features are advertised only if tested and supported. New fallback work must pass CPU-versus-GPU correctness, hardware timing, bounded memory usage and second-app integration gates.

Not a transparent driver, replacement Direct3D runtime or protected-game injection.
