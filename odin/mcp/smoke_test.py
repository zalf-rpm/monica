"""End-to-end smoke test: starts server.py over stdio like an MCP client
(Claude Code / Desktop) would, and drives a small scenario comparison."""

from __future__ import annotations

import json
import sys

import anyio
from mcp.client.client import Client, StdioServerParameters


def payload(result) -> object:
    if getattr(result, "structured_content", None) is not None:
        return result.structured_content
    if getattr(result, "structuredContent", None) is not None:
        return result.structuredContent
    return json.loads(result.content[0].text)


async def main() -> int:
    params = StdioServerParameters(command=sys.executable, args=["server.py"])
    async with Client(params) as client:
        tools = await client.list_tools()
        print("tools:", [t.name for t in tools.tools])
        crops = payload(await client.call_tool("list_crops", {}))
        print("species:", len(crops["result"] if isinstance(crops, dict) else crops))

        wheat = {"species": "wheat", "cultivar": "winter-wheat", "sowing_date": "09-22",
                 "latest_harvest_date": "09-05"}
        job_ids = []
        for kg in (0, 160):
            fert = [] if kg == 0 else [{"date": "03-15", "kg_n_ha": kg / 4}, {"date": "04-15", "kg_n_ha": kg / 2},
                                       {"date": "05-15", "kg_n_ha": kg / 4}]
            r = await client.call_tool("submit_simulation", {
                "rotation": [{**wheat, "n_fertilization": fert}], "label": f"WW {kg} kg N"})
            if r.is_error if hasattr(r, "is_error") else r.isError:
                print("submit failed:", r.content[0].text)
                return 1
            job = payload(r)
            print("job:", job["job_id"], job["status"], job.get("runtime_s"))
            job_ids.append(job["job_id"])

        cmp = payload(await client.call_tool("compare_jobs", {"job_ids": job_ids}))
        for j in cmp["jobs"]:
            print(j["label"], "mean yield:", j["mean_yield_by_crop_kg_ha"], "yearly:", j["mean_yearly"])

        bad = await client.call_tool("submit_simulation", {
            "rotation": [{**wheat, "cultivar": "no-such-cultivar"}]})
        print("invalid input ->", (bad.content[0].text[:120]))

        res = await client.read_resource(f"monica://jobs/{job_ids[1]}/out_section_crop.csv")
        print("resource bytes:", len(res.contents[0].text))
        prompts = await client.list_prompts()
        print("prompts:", [p.name for p in prompts.prompts])
    return 0


if __name__ == "__main__":
    sys.exit(anyio.run(main))
