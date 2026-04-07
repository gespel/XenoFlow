from flask import Flask, send_from_directory
from requests import get
from threading import Thread, Event

previous_data = None
data = {"packets_per_second": 0, "traffic_series": [], "num_backends": 0}

class DataGatherer(Thread):
	def __init__(self, event):
		Thread.__init__(self)
		self.stopped = event

	def run(self):
		global previous_data, data
		while not self.stopped.wait(5):
			try:
				response = get("http://localhost:8080/api")
				response.raise_for_status()
				#print(f"Fetched data: {response.json()}")

				if previous_data is None:
					previous_data = response.json()
				else:
					data["packets_per_second"] = 0
					for backend in response.json().get("backends", []):
						previous_backend = next((b for b in previous_data.get("backends", []) if b["name"] == backend["name"]), None)
						data["packets_per_second"] += (int(backend["packetsProcessed"]) - int(previous_backend["packetsProcessed"])) / 5
					#print(f"Calculated PPS: {data['packets_per_second']}")
					data["traffic_series"].append(data["packets_per_second"])
					if len(data["traffic_series"]) > 24:
						data["traffic_series"].pop(0)
					data["num_backends"] = len(response.json().get("backends", []))
					previous_data = response.json()

				
			except Exception as e:
				print(f"Error fetching data: {e}")



app = Flask(__name__, static_folder="static", static_url_path="/static")


@app.get("/")
def dashboard() -> str:
	"""Serve the frontend dashboard shell."""
	return send_from_directory(app.static_folder, "index.html")

@app.get("/api")
def api() -> str:
	try:
		return data
	except Exception as e:
		return {"error": str(e)}, 500

@app.get("/healthz")
def healthz() -> dict[str, str]:
	"""Simple liveness endpoint for local checks."""
	return {"status": "ok"}


if __name__ == "__main__":
	dt = DataGatherer(Event())
	dt.start()
	app.run(host="0.0.0.0", port=5000, debug=True, use_reloader=False)
