using Xunit.Sdk;
using Xunit.v3;

// Release-date tests change process environment; packaging also reads that environment.
[assembly: Parallelization(Mode = ParallelMode.None)]
