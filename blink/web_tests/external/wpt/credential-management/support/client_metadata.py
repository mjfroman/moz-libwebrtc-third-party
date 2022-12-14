def main(request, response):
  if not b"sec-fetch-dest" in request.headers or request.headers[b"sec-fetch-dest"] != b"webidentity":
    return (500, [], "Missing Sec-Fetch-Dest header")
  if b"cookie" in request.cookies:
    return (500, [], "Cookie should not be sent to this endpoint")
  return """
{
  "privacy_policy_url": "https://privacypolicy.com"
}
"""
