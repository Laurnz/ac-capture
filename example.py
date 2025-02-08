from datetime import datetime
from multiprocessing import Process, Queue
import time
from PIL import Image
from accapture import get_frame

def grab(queue: Queue) -> None:
    while "the game is running":
        image = get_frame()
        timestamp = datetime.now().strftime('%Y-%m-%d_%H-%M-%S-%f')
        queue.put((image, timestamp))

    # Tell the other worker to stop
    queue.put(None)


def save(queue: Queue) -> None:
    while "there are screenshots":
        img = queue.get()
        if img is None:
            break

        image, timestamp = img
        image.thumbnail((520, 520), Image.Resampling.LANCZOS) # Create smaller image
        image.save(f"{timestamp}.png")

        print(f"Queue size: {queue.qsize()}")


if __name__ == "__main__":
    # The screenshots queue
    queue: Queue = Queue()

    # 2 processes: one for grabing and one for saving PNG files
    Process(target=grab, args=(queue,)).start()
    Process(target=save, args=(queue,)).start()

    # In order to clear the queue quicker, multiple processes can be started
    #Process(target=save, args=(queue,)).start()
    #Process(target=save, args=(queue,)).start()
    #Process(target=save, args=(queue,)).start()
    #Process(target=save, args=(queue,)).start()
    #Process(target=save, args=(queue,)).start()
