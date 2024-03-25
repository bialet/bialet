document.body.addEventListener('click', event => {
  if (event.target.tagName.toLowerCase() === 'a') {
    event.preventDefault()
    bialet.openExternal(event.target.href)
  }
})
