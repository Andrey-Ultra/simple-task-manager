import json
import logging
import time
from dataclasses import dataclass

from . import _http
from .errors import ServerError

log = logging.getLogger("stm")


@dataclass(frozen=True)
class Job:
    """A task given to the worker. This id is not the same as the client id."""
    id: int
    task: str


class Worker:
    """Worker: takes tasks from the queue and sends back answers."""

    def __init__(self, url="http://localhost:8080", timeout=10.0):
        self.url = url
        self.timeout = timeout

    def _request(self, method, path, body=None):
        return _http.request(self.url, method, path, body, self.timeout)

    def take(self):
        """Take a task. Return None if the queue is empty."""
        code, text = self._request("POST", "/worker/take")
        if code == 204:
            return None
        if code == 200:
            data = json.loads(text)
            return Job(id=data["id"], task=data["task"])
        raise ServerError(f"unexpected response {code}: {text}")

    def _finish(self, path, body=None) -> bool:
        code, text = self._request("POST", path, body)
        if code == 200:
            return True
        if code == 409:                       # the task is not in work: we were too late
            return False
        raise ServerError(f"unexpected response {code}: {text}")

    def answer(self, job_id: int, text: str) -> bool:
        """Send the answer. Return False if the task is not ours anymore (timeout)."""
        return self._finish(f"/worker/tasks/{job_id}/answer", text)

    def release(self, job_id: int) -> bool:
        """Put the task back to the queue without an answer."""
        return self._finish(f"/worker/tasks/{job_id}/release")

    def run(self, handler, poll=1.0):
        """Work in a loop: take a task, call handler(task) and send the answer.

        If the handler fails, the task goes back to the queue. Stop with Ctrl+C.
        """
        try:
            while True:
                job = self.take()
                if job is None:
                    time.sleep(poll)
                    continue
                try:
                    result = handler(job.task)
                except Exception:
                    log.exception("handler failed on job %s, releasing", job.id)
                    self.release(job.id)
                    continue
                if not self.answer(job.id, str(result)):
                    log.warning("job %s expired before the answer was sent", job.id)
        except KeyboardInterrupt:
            pass
