guard(null).then((me) => {
  if (me) location.href = roleHome(me.role);
  else location.href = "/login.html";
});
