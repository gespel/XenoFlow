from flask import Flask, send_from_directory
from requests import get


app = Flask(__name__, static_folder="static", static_url_path="/static")


@app.get("/")
def dashboard() -> str:
	"""Serve the frontend dashboard shell."""
	return send_from_directory(app.static_folder, "index.html")

@app.get("/api")
def api() -> dict[str, str]:
	try:
		response = get("http://localhost:8080/api")
		response.raise_for_status()
		return {"metrics": response.text}
	except Exception as e:
		return {"error": str(e)}, 500

@app.get("/healthz")
def healthz() -> dict[str, str]:
	"""Simple liveness endpoint for local checks."""
	return {"status": "ok"}


if __name__ == "__main__":
	app.run(host="0.0.0.0", port=5000, debug=True)
