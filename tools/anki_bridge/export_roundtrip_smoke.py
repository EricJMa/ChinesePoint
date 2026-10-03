#!/usr/bin/env python3
"""Import a real reader export into a temporary Anki collection and check it.

Uses Anki's Collection API from the running Python environment (for example
`pip install anki` in a throwaway venv, matching the Anki Desktop version).
It never opens an Anki profile. Checks:

1. Every exported word with an answer becomes exactly one card with the
   exported sentence and answer.
2. Re-sending the same batch and sending a later batch with the same words
   creates no duplicate notes or cards.
3. After a review, a later import updates fields but leaves Anki's schedule
   for that card untouched.

    python export_roundtrip_smoke.py --export /path/to/learner-v1.jsonl \
        --expect "明月=床前明月光，疑是地上霜。"
"""

from __future__ import annotations

import argparse
import html
import http.client
import json
import tempfile
from pathlib import Path

from real_collection_smoke import CLIENT_ID, TOKEN, free_port, load_server

SCHEDULE_FIELDS = ("type", "queue", "due", "ivl", "factor", "reps", "lapses")


def post(server, port: int, batch: str, payload: bytes) -> dict:
    request = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
    request.request(
        "POST",
        server.ENDPOINT,
        body=payload,
        headers={
            "Authorization": f"Bearer {TOKEN}",
            "Content-Type": "application/x-ndjson",
            "X-ChinesePoint-Client": CLIENT_ID,
            "X-ChinesePoint-Batch": batch,
        },
    )
    response = request.getresponse()
    result = {"status": response.status, "payload": json.loads(response.read())}
    request.close()
    return result


def check(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)
    print(f"  ok: {message}")


def schedule(card) -> dict:
    return {name: getattr(card, name) for name in SCHEDULE_FIELDS}


def answer_good(collection, card) -> None:
    from anki.scheduler.v3 import CardAnswer

    sched = collection.sched
    card.start_timer()
    states = collection._backend.get_scheduling_states(card.id)
    sched.answer_card(sched.build_answer(card=card, states=states, rating=CardAnswer.GOOD))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--export", type=Path, required=True, help="learner-v1.jsonl written by the reader")
    parser.add_argument("--expect", action="append", default=[], metavar="WORD=SENTENCE",
                        help="require WORD to be exported with exactly SENTENCE (repeatable)")
    args = parser.parse_args()
    required = dict(item.split("=", 1) for item in args.expect)

    from anki.collection import Collection

    payload = args.export.read_bytes()
    server = load_server()
    expected = [record for record in server.parse_vocabulary_ndjson(payload) if record.answer]
    check(len(expected) > 0, f"export has {len(expected)} word(s) with a dictionary answer")
    exported = {record.headword: record.sentence for record in expected}
    for word, sentence in required.items():
        check(exported.get(word) == sentence, f"export has {word} with exactly {sentence}")

    config = {"port": free_port(), "token": TOKEN, "processed_batches": []}
    with tempfile.TemporaryDirectory(prefix="chinesepoint-anki-roundtrip-") as temporary:
        collection = Collection(str(Path(temporary) / "collection.anki2"))
        bridge = server.BridgeServer(lambda: config, lambda value: config.update(value),
                                     lambda callback: callback(), lambda: collection)
        bridge.start()
        try:
            port = config["port"]
            first = post(server, port, f"cp-v1-{CLIENT_ID}-1-1", payload)
            check(first["status"] == 200 and first["payload"]["added"] == len(expected),
                  f"first import added {first['payload'].get('added')} card(s)")

            for record in expected:
                note_ids = collection.find_notes(f'"ChinesePointId:{record.word_id}"')
                check(len(note_ids) == 1, f"{record.headword}: one note")
                note = collection.get_note(note_ids[0])
                check(html.unescape(note["Word"]) == record.headword, f"{record.headword}: word field")
                check(html.unescape(note["Sentence"]) == record.sentence,
                      f"{record.headword}: sentence is {record.sentence}")
                check(html.unescape(note["Answer"]) == record.answer, f"{record.headword}: dictionary answer kept")

            retry = post(server, port, f"cp-v1-{CLIENT_ID}-1-1", payload)
            again = post(server, port, f"cp-v1-{CLIENT_ID}-2-1", payload)
            check(retry["payload"] == {"batch_id": f"cp-v1-{CLIENT_ID}-1-1", "added": 0, "updated": 0},
                  "re-sending the same batch changes nothing")
            check(again["payload"]["added"] == 0, "a later batch with the same words adds nothing")
            check(len(collection.find_notes(f'"note:{server.MODEL_NAME}"')) == len(expected), "no duplicate notes")
            check(len(collection.find_cards(f'"note:{server.MODEL_NAME}"')) == len(expected), "no duplicate cards")

            card_id = collection.find_cards(f'"ChinesePointId:{expected[0].word_id}"')[0]
            card = collection.get_card(card_id)
            before_review = schedule(card)
            answer_good(collection, card)
            reviewed = schedule(collection.get_card(card_id))
            check(reviewed != before_review and reviewed["reps"] == before_review["reps"] + 1,
                  f"review recorded (reps {before_review['reps']} -> {reviewed['reps']})")

            later = post(server, port, f"cp-v1-{CLIENT_ID}-3-1", payload)
            check(later["payload"]["added"] == 0, "import after review adds nothing")
            check(schedule(collection.get_card(card_id)) == reviewed, "import after review keeps Anki's schedule")
            print("export round trip passed")
        finally:
            if bridge._httpd is not None:
                bridge._httpd.shutdown()
                bridge._httpd.server_close()
            if bridge._thread is not None:
                bridge._thread.join(timeout=2)
            collection.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
