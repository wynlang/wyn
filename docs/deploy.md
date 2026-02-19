# Deploying Wyn Applications

Deploy your Wyn app to any server with one command.

## Setup

1. Add a `[deploy]` section to your `wyn.toml`:

```toml
[project]
name = "myapp"
version = "1.0.0"
entry = "src/main.wyn"

[deploy.dev]
host = "dev.example.com"
user = "deploy"
key = "~/.ssh/id_ed25519"
path = "/opt/myapp"
os = "linux"
pre = "systemctl stop myapp"
post = "systemctl start myapp"

[deploy.prod]
host = "example.com"
user = "deploy"
key = "~/.ssh/id_ed25519"
path = "/opt/myapp"
os = "linux"
pre = "systemctl stop myapp"
post = "systemctl start myapp"
```

2. Set up SSH key access to your server:

```bash
ssh-keygen -t ed25519                    # Generate key (if needed)
ssh-copy-id deploy@dev.example.com       # Copy to server
```

3. Create the app directory on the server:

```bash
ssh deploy@dev.example.com "sudo mkdir -p /opt/myapp && sudo chown deploy /opt/myapp"
```

## Deploy

```bash
wyn deploy dev          # Deploy to dev server
wyn deploy prod         # Deploy to production
wyn deploy dev --dry-run  # Preview without executing
```

## What happens

1. Cross-compiles your app for the target OS (`os` field)
2. Runs the `pre` command on the server (e.g., stop the service)
3. Uploads the binary via SCP
4. Runs the `post` command (e.g., start the service)

## Other commands

```bash
wyn ssh dev             # SSH into the dev server
wyn logs dev            # Tail the service logs
```

## Systemd service (optional)

Create `/etc/systemd/system/myapp.service` on the server:

```ini
[Unit]
Description=My Wyn App

[Service]
ExecStart=/opt/myapp/myapp
Restart=always
User=deploy

[Install]
WantedBy=multi-user.target
```

Then: `sudo systemctl enable myapp`
