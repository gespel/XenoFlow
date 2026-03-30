from flask import Flask, send_from_directory


app = Flask(__name__, static_folder="static", static_url_path="/static")


@app.get("/")
def dashboard() -> str:
	"""Serve the frontend dashboard shell."""
	return send_from_directory(app.static_folder, "index.html")


@app.get("/healthz")
def healthz() -> dict[str, str]:
	"""Simple liveness endpoint for local checks."""
	return {"status": "ok"}


if __name__ == "__main__":
	app.run(host="0.0.0.0", port=5000, debug=True)
