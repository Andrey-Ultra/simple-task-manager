import urllib.error
import urllib.request

from .errors import ServerError


def request(base_url, method, path, body=None, timeout=10.0):
    """Send a request and return (status code, text).

    Codes 4xx and 5xx are not errors here: the caller checks them.
    ServerError is raised only when we cannot reach the server.
    """
    data = (body or "").encode("utf-8") if method == "POST" or body is not None else None
    req = urllib.request.Request(
        base_url.rstrip("/") + path,
        data=data,
        method=method,
        headers={"Content-Type": "text/plain; charset=utf-8"},
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8")
    except (urllib.error.URLError, TimeoutError, ConnectionError) as e:
        raise ServerError(f"cannot reach {base_url}: {e}") from e
