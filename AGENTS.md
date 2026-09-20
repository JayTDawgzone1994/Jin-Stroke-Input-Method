# Repository workflow

- Use `develop` for ongoing development and fixes. Base feature branches on `develop` and target pull requests at `develop`.
- Reserve `main` for releases. Do not commit routine development directly to `main`.
- Keep completed changes and local commits on this machine. Push to a remote only when the user explicitly requests a push; completing development or making a local commit does not authorize pushing.
- Merge validated changes into `main` and create version tags / GitHub Releases only when release work is requested.
- See `CONTRIBUTING.md` for contribution and validation requirements.
