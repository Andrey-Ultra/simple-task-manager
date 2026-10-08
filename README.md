# simple-task-manager

Простая очередь задач на C++ с HTTP-API и Python-клиентом.
Клиенты отправляют задачи, воркеры забирают их и возвращают решения. Если воркер завис, задача сама возвращается в очередь.

Задача для сервера — просто строка (текст, JSON, ссылка). Что с ней делать, решают клиент и воркер.

Статусы задачи: `in queue` → `in work` → `complete`.

## Запуск

**Docker:**
```bash
docker build -t simple-task-manager .
docker run --rm -p 8080:8080 -e TASK_TIMEOUT=60 simple-task-manager
```

**Из исходников** (нужны CMake ≥ 3.24 и компилятор с C++20, зависимости скачаются сами):
```bash
cmake -S . -B build && cmake --build build
./build/simple_task_manager
```

Настройки через переменные окружения:

| Переменная | По умолчанию | Описание |
|---|---|---|
| `HOST` | `0.0.0.0` | адрес сервера |
| `PORT` | `8080` | порт |
| `TASK_TIMEOUT` | `300` | сколько секунд воркер может держать задачу |

## HTTP-API

**Клиент**

| Запрос | Описание | Ответ |
|---|---|---|
| `POST /tasks` | Отправить задачу (тело — текст задачи) | `200` + `id`; `400`, если тело пустое |
| `GET /tasks/{id}/status` | Узнать статус | `200` + статус; `404`, если задачи нет |
| `GET /tasks/{id}/answer` | Забрать ответ (после этого задача удаляется) | `200` + ответ; `404`, если ответа ещё нет |
| `DELETE /tasks/{id}` | Отменить задачу | `200`; `404`, если задачи нет |

**Воркер**

| Запрос | Описание | Ответ |
|---|---|---|
| `POST /worker/take` | Взять задачу | `200` + `{"id": 7, "task": "..."}`; `204`, если очередь пуста |
| `POST /worker/tasks/{id}/answer` | Вернуть решение (тело — ответ) | `200`; `409`, если задача не в работе |
| `POST /worker/tasks/{id}/release` | Вернуть задачу в очередь | `200`; `409`, если задача не в работе |

`id` воркера приходит из `/worker/take` и отличается от `id` клиента. Если воркер не успел за таймаут, задача возвращается в очередь с новым `id`, а его запоздавший ответ получает `409`.

```bash
curl -X POST localhost:8080/tasks -d '2+2'                         # → 0
curl -X POST localhost:8080/worker/take                            # → {"id":0,"task":"2+2"}
curl -X POST localhost:8080/worker/tasks/0/answer -d '4'
curl localhost:8080/tasks/0/answer                                 # → 4
```

## Python-клиент

Без внешних зависимостей, нужен Python ≥ 3.9.

```bash
pip install "git+https://github.com/Andrey-Ultra/simple-task-manager.git#subdirectory=python"
```

```python
from stm import Client, Worker

# клиент
c = Client("http://localhost:8080")
task_id = c.submit("21")
print(c.wait(task_id, timeout=30))        # ждёт и возвращает ответ

# воркер (в другом процессе)
Worker("http://localhost:8080").run(lambda task: int(task) * 2)
```

| Метод | Описание |
|---|---|
| `Client.submit(task)` | отправить задачу, вернуть `id` |
| `Client.status(id)` | статус задачи |
| `Client.answer(id)` | забрать ответ или `None`, если его ещё нет |
| `Client.wait(id, timeout=None)` | ждать ответ; по истечении времени `TimeoutError` |
| `Client.cancel(id)` | отменить задачу |
| `Worker.take()` | взять задачу (`Job(id, task)`) или `None` |
| `Worker.answer(job_id, text)` | вернуть решение; `False`, если опоздал |
| `Worker.release(job_id)` | вернуть задачу в очередь |
| `Worker.run(handler)` | цикл «взять → `handler(task)` → ответить»; при ошибке задача возвращается в очередь |

Если задачи нет, методы бросают `TaskNotFound`; если сервер недоступен — `ServerError`.

## Ограничения

- Данные хранятся в памяти и пропадают при перезапуске.
- Нет авторизации: сервер рассчитан на доверенную сеть.
