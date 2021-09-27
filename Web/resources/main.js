/*
var session_id = null
fetch('/query', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    action: "new_session"
  })
}).then(response => response.json()).then(data => session_id = data.session_id).catch(update_error);

function query(data) {
  data["session_id"] = session_id
  return fetch('/query', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json'
    },
    body: JSON.stringify(data)
  }).then(response => response.json()).then(update).catch(update_error);
}
*/
