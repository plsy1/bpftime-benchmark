import json,sys
d=json.load(sys.stdin)
print("#%s %s [%s] by %s created %s updated %s closed %s"%(d.get("number"),d.get("title"),d.get("state"),(d.get("author") or {}).get("login"),d.get("createdAt"),d.get("updatedAt"),d.get("closedAt")))
lab=d.get("labels")
if lab: print("LABELS:",[l["name"] for l in lab])
print("BODY:\n"+(d.get("body") or ""))
for c in d.get("comments",[]):
  print("--- COMMENT by %s %s\n%s"%((c.get("author") or {}).get("login"),c.get("createdAt"),c.get("body")))
