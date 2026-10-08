class STMError(Exception):
    """Base error of the library."""


class TaskNotFound(STMError):
    """The task does not exist (or it was already taken or cancelled)."""


class ServerError(STMError):
    """The server is not available or sent an unexpected answer."""
