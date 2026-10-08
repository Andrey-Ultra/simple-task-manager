import time

from . import _http
from .errors import ServerError, TaskNotFound

IN_QUEUE = "in queue"
IN_WORK = "in work"
COMPLETE = "complete"


class Client:
    """Client of the queue: sends tasks and gets answers."""

    def __init__(self, url="http://localhost:8080", timeout=10.0):
        self.url = url
        self.timeout = timeout

    def _request(self, method, path, body=None):
        return _http.request(self.url, method, path, body, self.timeout)

    def submit(self, task: str) -> int:
        """Send a task and get its id."""
        code, text = self._request("POST", "/tasks", task)
        if code == 200:
            return int(text)
        if code == 400:
            raise ValueError("task must not be empty")
        raise ServerError(f"unexpected response {code}: {text}")

    def status(self, task_id: int) -> str:
        """Get the status: 'in queue', 'in work' or 'complete'."""
        code, text = self._request("GET", f"/tasks/{task_id}/status")
        if code == 200:
            return text
        if code == 404:
            raise TaskNotFound(task_id)
        raise ServerError(f"unexpected response {code}: {text}")

    def answer(self, task_id: int):
        """Get the answer, or None if there is no answer yet.

        After a successful call the task is removed from the server.
        You cannot get the same answer twice.
        """
        code, text = self._request("GET", f"/tasks/{task_id}/answer")
        if code == 200:
            return text
        if code == 404:
            return None
        raise ServerError(f"unexpected response {code}: {text}")

    def cancel(self, task_id: int) -> None:
        """Cancel the task in any status."""
        code, text = self._request("DELETE", f"/tasks/{task_id}")
        if code == 404:
            raise TaskNotFound(task_id)
        if code != 200:
            raise ServerError(f"unexpected response {code}: {text}")

    def wait(self, task_id: int, timeout=None, poll=0.5) -> str:
        """Wait until the task is ready and return the answer.

        timeout=None means wait forever. If time is over, raise TimeoutError.
        """
        deadline = None if timeout is None else time.monotonic() + timeout
        while True:
            if self.status(task_id) == COMPLETE:
                answer = self.answer(task_id)
                if answer is None:            # somebody else took the answer
                    raise TaskNotFound(task_id)
                return answer
            if deadline is not None and time.monotonic() >= deadline:
                raise TimeoutError(f"task {task_id} not ready after {timeout}s")
            time.sleep(poll)
