# web-serv-Tester

An interactive HTTP tester for the 42 webserv project.

This tester does not start your server. You must run your own server first, then run this tester against it.

## Mandatory First Step (Do This Before Anything)

You must copy the config + fixture files from this tester into your server workspace, then adapt paths/directives to your implementation.

Required source content in this repository:

- `EngineX/EngineX.conf`
- `EngineX/www/`
- `EngineX/www-subject-tester/`

Example copy workflow:

```bash
# from tester repository root
cp -r EngineX /path/to/your-webserv-project/

# then edit config inside your server project
cd /path/to/your-webserv-project
$EDITOR EngineX/EngineX.conf
```

What you must adapt in the copied config so it works with your server:

1. Root paths
- Make sure roots point to the copied fixture folders (`EngineX/www` and `EngineX/www-subject-tester`) in your server workspace.

2. CGI execution mapping
- Ensure `.py` routes execute with your Python interpreter (usually `python3`).
- Ensure `EngineX/www-subject-tester/cgi_tester` is executable if your server requires it.

3. Required listening ports
- Port `1025`: main suites.
- Port `1026`: alternate-port test in Happy Path suite.
- Port `1027`: Minimum Evaluation suite.

4. Route behavior compatibility
- Keep route semantics needed by tests: redirects, method restrictions, upload routes, CGI routes, body size limits, and autoindex behavior.

5. Permissions
- If your server requires executable CGI files, run:

```bash
chmod +x EngineX/www/cgi-bin/*.py
chmod +x EngineX/www-subject-tester/cgi_tester
```

## Quick Start

1. Build the tester:

```bash
make re
```

2. Build and run your server in a separate terminal, using your adapted copied config.

3. Start the tester:

```bash
./servTester.out
```

4. Use the interactive menu.

## How To Use (Most Important)

The tester is menu-driven.

Main menu options:

- `0` runs all suites.
- `1..13` opens a specific suite.
- last option exits.

Input format supports:

- Single choice: `3`
- Multiple choices: `1 3 5`
- Range: `1-5`

Inside a suite:

- `0` runs all tests in that suite.
- `1..N` runs selected test(s).
- last option returns to main menu.

After runs, press Enter when prompted to continue.

Single-test detail mode:

When you run one test (not multi-select), you can inspect:

- Tester request body
- Server response body
- test configuration notes
- re-run the same test

This is the fastest way to debug a failing case.

## Current Suite Order

1. Happy Path Tests
2. Error Handling Tests
3. Client Body Size Tests
4. Method Restriction Tests
5. Redirection Tests
6. Auto Index Tests
7. Routing Tests
8. CGI Tests
9. Chunked Transfer Tests
10. Session Tests
11. Stress Tests
12. Path Tests
13. Minimum Evaluation Tests

## Recommended Run Strategy

1. Start with smoke suites
- Happy Path, Error Handling, Method Restriction, Routing.

2. Then protocol-heavy suites
- CGI, Chunked Transfer, Session.

3. Run heavy suites last
- Stress and Minimum Evaluation (can be very long and resource intensive).

## Important Port Dependencies

- Most tests use `localhost:1025`.
- Happy Path includes a test that expects `localhost:1026`.
- Minimum Evaluation tests expect `127.0.0.1:1027` and specific subject-style routes/files.

If a suite fails with connection errors, check that the correct port is actually listened to by your server.

## Common Failure Causes

1. Immediate connection failure
- Server not running or not listening on expected port(s).

2. Timeout waiting for response
- Server did not send a complete response framing (`Content-Length` or valid chunked termination), or kept connection open unexpectedly.

3. CGI tests fail
- Wrong CGI route mapping, wrong script path, missing execute permission, or wrong Python interpreter path.

4. Session tests fail
- Cookie name mismatch with your implementation can break assertions.

5. Path/permission tests behave unexpectedly
- Running server as root can bypass permission checks and invalidate 403 scenarios.

## Notes

- `README.md` in this repo describes tester usage.
- The tester expects your server behavior to match the copied/adapted scenario config and fixtures.
- If you modify suite content, menu numbering may change.
