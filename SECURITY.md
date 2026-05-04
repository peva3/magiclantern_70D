# Security Policy

## Reporting a Vulnerability

Magic Lantern is open-source firmware enhancement software for Canon DSLR cameras. As firmware that runs directly on camera hardware, security issues should be taken seriously.

If you discover a security vulnerability:

1. **Do not** open a public GitHub issue
2. Email the repository maintainer at **pmwoodward3@gmail.com**
3. Include a detailed description of the issue and steps to reproduce

You should receive a response within 48 hours. If you don't, please follow up.

## What to Expect

- I will acknowledge receipt within 2 business days
- I will investigate and provide an assessment within 5 business days
- For confirmed vulnerabilities, I will work on a fix and coordinated disclosure

## Scope

This security policy covers:
- The Magic Lantern firmware binary (`autoexec.bin`)
- All included modules (`.mo` files)
- Companion tools (`camremote.py`, `camtunnel.py`)
- Build system and scripts

## Out of Scope

- Canon's proprietary firmware itself (report to Canon directly)
- Physical camera hardware vulnerabilities
- Issues arising from deliberate modification of the camera

## Preferred Reporting Style

Please include:
- The version/commit hash of the affected code
- Camera model and firmware version (e.g., Canon 70D, FW 1.1.2)
- A clear description of the vulnerability
- Steps to reproduce
- Any proof-of-concept code (if applicable)

## Responsible Disclosure

I request a 90-day grace period from the time of notification before public disclosure, to allow time for a fix to be developed and deployed.

Thank you for helping keep Magic Lantern safe.
